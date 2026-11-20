"""F4 (19 Nov 2026): Blender builds for the hull classes with no Bridge Commander model and no free download.

    blender -b --factory-startup --python f4_blender_builds.py -- --out <LocalAssets/incoming/f4> [--keys k1,k2] [--bc H:/StarhelmTools/bc-ships]

Two kinds of build, both written as sources for hullhd_build.py (which orients, scales to canon length, bakes and exports the glb):
  * modelled: an original hull built from primitives and modifiers here (Xindi, Suliban, Nausicaan, the Bajoran lightship,
    stations, the space-borne threats). Written as <out>/<key>/SM_<slug>.obj, untextured; hullhd bakes the faction plating.
  * kitbash: a Bridge Commander model we already use, cut and recombined into a new class (the Ferengi line from the Marauder
    and the Merchantman, the Cardassian freighter variants, the Romulan drone ship). Material names are kept, so hullhd binds
    the source ship's own HD texture maps. Written as <out>/<key>/SM_<slug>.fbx.
Every build is nose +X, top +Z, centred; size is free (hullhd scales to canon length). Private, local only.
"""
import math
import os
import random
import sys

import bmesh
import bpy
from mathutils import Euler, Matrix, Vector


def arg(name, default=None):
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    return argv[argv.index(name) + 1] if name in argv else default


OUT = arg("--out")
BC = arg("--bc", "H:/StarhelmTools/bc-ships")
ONLY = set(filter(None, (arg("--keys") or "").split(",")))


# ---------------------------------------------------------------------------------------------------------------------
# Primitive helpers (each returns the new object; everything is joined at the end)
# ---------------------------------------------------------------------------------------------------------------------
def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def _obj(me, name="part"):
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    return ob


def place(ob, loc=(0, 0, 0), rot=(0, 0, 0), scale=(1, 1, 1)):
    ob.location = Vector(loc)
    ob.rotation_euler = Euler([math.radians(r) for r in rot])
    ob.scale = Vector(scale)
    return ob


def uvsphere(loc=(0, 0, 0), scale=(1, 1, 1), rot=(0, 0, 0), seg=32, rings=16):
    me = bpy.data.meshes.new("s")
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=seg, v_segments=rings, radius=1.0)
    bm.to_mesh(me); bm.free()
    return place(_obj(me), loc, rot, scale)


def cyl(loc=(0, 0, 0), scale=(1, 1, 1), rot=(0, 0, 0), seg=24, r1=1.0, r2=1.0, depth=2.0):
    me = bpy.data.meshes.new("c")
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, cap_tris=False, segments=seg, radius1=r1, radius2=r2, depth=depth)
    bm.to_mesh(me); bm.free()
    return place(_obj(me), loc, rot, scale)


def box(loc=(0, 0, 0), scale=(1, 1, 1), rot=(0, 0, 0), bevel=0.0):
    me = bpy.data.meshes.new("b")
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=2.0)
    bm.to_mesh(me); bm.free()
    ob = place(_obj(me), loc, rot, scale)
    if bevel:
        m = ob.modifiers.new("bv", "BEVEL"); m.width = bevel; m.segments = 2; m.affect = "EDGES"
    return ob


def torus(loc=(0, 0, 0), rot=(0, 0, 0), R=1.0, r=0.1, seg=48, mseg=12, arc=360.0, scale=(1, 1, 1)):
    """A torus or an arc of one (arc degrees, centred on +X), flattened by scale."""
    me = bpy.data.meshes.new("t")
    bm = bmesh.new()
    rings = []
    n = seg + (0 if arc >= 360 else 1)
    for i in range(n):
        a = math.radians(-arc / 2 + arc * i / seg)
        c = Vector((R * math.cos(a), R * math.sin(a), 0))
        out = Vector((math.cos(a), math.sin(a), 0))
        ring = []
        for j in range(mseg):
            b = 2 * math.pi * j / mseg
            ring.append(bm.verts.new(c + out * (r * math.cos(b)) + Vector((0, 0, r * math.sin(b)))))
        rings.append(ring)
    for i in range(len(rings) - (0 if arc >= 360 else 1)):
        a, b = rings[i], rings[(i + 1) % len(rings)]
        for j in range(mseg):
            bm.faces.new((a[j], a[(j + 1) % mseg], b[(j + 1) % mseg], b[j]))
    if arc < 360:
        bm.faces.new(list(reversed(rings[0]))); bm.faces.new(rings[-1])
    bm.to_mesh(me); bm.free()
    return place(_obj(me), loc, rot, scale)


def loft(sections, closed_ends=True, seg=16):
    """A hull lofted through cross-sections [(x, half_width, half_height, z_offset, squareness 0..1)], nose at the first."""
    me = bpy.data.meshes.new("l")
    bm = bmesh.new()
    rings = []
    for x, w, h, z, sq in sections:
        ring = []
        for j in range(seg):
            a = 2 * math.pi * j / seg
            c, s = math.cos(a), math.sin(a)
            # superellipse: squareness pushes towards a box
            e = 2.0 / (2.0 + 6.0 * sq)
            y = w * math.copysign(abs(c) ** e, c)
            zz = h * math.copysign(abs(s) ** e, s)
            ring.append(bm.verts.new((x, y, z + zz)))
        rings.append(ring)
    for i in range(len(rings) - 1):
        a, b = rings[i], rings[i + 1]
        for j in range(seg):
            bm.faces.new((a[j], b[j], b[(j + 1) % seg], a[(j + 1) % seg]))
    if closed_ends:
        bm.faces.new(list(reversed(rings[0]))); bm.faces.new(rings[-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(me); bm.free()
    return _obj(me)


def wing(root, tip, chord_root, chord_tip, thick=0.04, sweep=0.0, dihedral=0.0):
    """A flat tapered wing from root (x, y, z) outwards to tip y; chord along x."""
    me = bpy.data.meshes.new("w")
    bm = bmesh.new()
    rx, ry, rz = root
    ty = tip
    tx = rx - sweep
    tz = rz + dihedral
    pts = [(rx + chord_root / 2, ry, rz), (rx - chord_root / 2, ry, rz), (tx - chord_tip / 2, ty, tz), (tx + chord_tip / 2, ty, tz)]
    top = [bm.verts.new((x, y, z + thick / 2)) for x, y, z in pts]
    bot = [bm.verts.new((x, y, z - thick / 2)) for x, y, z in pts]
    bm.faces.new(top); bm.faces.new(list(reversed(bot)))
    for i in range(4):
        j = (i + 1) % 4
        bm.faces.new((top[i], bot[i], bot[j], top[j]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(me); bm.free()
    return _obj(me)


def mirror_y(ob):
    m = ob.modifiers.new("my", "MIRROR"); m.use_axis = (False, True, False); m.use_clip = False
    return ob


def displace(ob, strength=0.1, size=0.5, seed=1):
    tex = bpy.data.textures.new("n%d" % seed, "CLOUDS"); tex.noise_scale = size
    sub = ob.modifiers.new("sub", "SUBSURF"); sub.levels = 2; sub.render_levels = 2
    d = ob.modifiers.new("d", "DISPLACE"); d.texture = tex; d.strength = strength; d.texture_coords = "OBJECT"
    return ob


def smooth(ob, levels=1):
    m = ob.modifiers.new("ss", "SUBSURF"); m.levels = levels; m.render_levels = levels
    return ob


def greeble(target_size, center, count, seed, size=(0.04, 0.12)):
    # 20 Nov: off. The boxes sit on a bounding cube, not on the hull, and floated clear of it in the in-engine frames.
    return []
    rnd = random.Random(seed)
    out = []
    for _ in range(count):
        s = [rnd.uniform(*size) * target_size for _ in range(3)]
        face = rnd.choice([(1, 0, 0), (-1, 0, 0), (0, 1, 0), (0, -1, 0), (0, 0, 1), (0, 0, -1)])
        p = [rnd.uniform(-0.5, 0.5) * target_size for _ in range(3)]
        for i in range(3):
            if face[i]:
                p[i] = face[i] * 0.5 * target_size
        out.append(box(tuple(c + q for c, q in zip(center, p)), tuple(x / 2 for x in s)))
    return out


def finish(path):
    """Applies modifiers, joins every mesh, recentres, writes OBJ."""
    obs = [o for o in bpy.context.scene.objects if o.type == "MESH"]
    dg = bpy.context.evaluated_depsgraph_get()
    for o in obs:
        bpy.context.view_layer.objects.active = o
        for m in list(o.modifiers):
            bpy.ops.object.modifier_apply(modifier=m.name)
    bpy.ops.object.select_all(action="DESELECT")
    for o in obs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = obs[0]
    bpy.ops.object.join()
    hull = bpy.context.view_layer.objects.active
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    mat = bpy.data.materials.new("M_Hull")
    hull.data.materials.clear(); hull.data.materials.append(mat)
    bpy.ops.object.shade_smooth_by_angle(angle=math.radians(40))
    os.makedirs(os.path.dirname(path), exist_ok=True)
    bpy.ops.wm.obj_export(filepath=path, forward_axis="Y", up_axis="Z", export_materials=False, export_uv=True,
                          export_normals=True, apply_modifiers=True)
    tris = sum(len(p.vertices) - 2 for p in hull.data.polygons)
    print("F4BUILD", os.path.basename(path), "tris", tris)


# ---------------------------------------------------------------------------------------------------------------------
# Modelled hulls (nose +X, top +Z; roughly 10 units long)
# ---------------------------------------------------------------------------------------------------------------------
def xindi_reptilian():
    # ENT "The Xindi": a long armoured dagger with twin forward mandibles, a dorsal crest and a broad aft engine block.
    h = loft([(5.0, 0.05, 0.05, 0, 0.3), (4.0, 0.45, 0.25, 0.05, 0.6), (2.0, 0.85, 0.42, 0.1, 0.7), (-1.5, 1.05, 0.55, 0.1, 0.75),
              (-3.6, 1.25, 0.6, 0.05, 0.8), (-5.0, 1.0, 0.5, 0.0, 0.85)], seg=20)
    for s in (1, -1):
        m = loft([(5.6, 0.04, 0.04, -0.15, 0.2), (4.6, 0.16, 0.12, -0.15, 0.5), (3.0, 0.24, 0.16, -0.1, 0.6), (1.6, 0.2, 0.14, -0.05, 0.6)], seg=12)
        m.location = (0, 0.95 * s, 0); m.rotation_euler = (0, 0, math.radians(-6 * s))
        e = cyl((-4.7, 1.25 * s, 0.05), rot=(0, 90, 0), r1=0.38, r2=0.32, depth=1.6, seg=20)
        wing((-2.6, 0.9 * s, -0.05), 2.4 * s, 2.2, 0.9, 0.08, sweep=1.0, dihedral=-0.35)
    crest = wing((-1.0, 0, 0.5), 0.0, 4.0, 1.4, 0.09)
    crest.rotation_euler = (math.radians(90), 0, 0); crest.location = (0, 0, 0.55); crest.scale = (1, 1, 1)
    c2 = box((-0.8, 0, 0.75), (2.4, 0.08, 0.28), bevel=0.04)
    greeble(1.6, (-2.5, 0, 0.3), 26, 11, (0.05, 0.14))


def xindi_insectoid():
    # A swept crescent hull, open at the bow, with a central command pod and rear vanes.
    t = torus((0, 0, 0), (0, 0, 180), R=3.2, r=0.55, seg=40, mseg=14, arc=230, scale=(1.0, 1.0, 0.45))
    t.location = (0.8, 0, 0)
    pod = loft([(1.4, 0.05, 0.05, 0, 0.1), (0.6, 0.6, 0.35, 0, 0.2), (-1.4, 0.75, 0.42, 0, 0.3), (-2.8, 0.4, 0.25, 0, 0.2)], seg=18)
    for s in (1, -1):
        wing((-2.2, 0.4 * s, 0.1), 1.6 * s, 1.0, 0.3, 0.06, sweep=0.8, dihedral=0.6)
        cyl((-0.2, 3.0 * s, 0), rot=(0, 90, 0), r1=0.22, r2=0.14, depth=1.8, seg=16)
    for o in (t, pod):
        smooth(o, 1)


def xindi_aquatic():
    # The large aquatic cruiser: a long, broad, ribbed ovoid with a raised spine and fin-like flanks.
    body = uvsphere((0, 0, 0), (5.0, 1.9, 0.75), seg=48, rings=24)
    spine = loft([(4.2, 0.05, 0.05, 0.55, 0.2), (2.0, 0.45, 0.32, 0.65, 0.4), (-3.0, 0.55, 0.35, 0.6, 0.5), (-4.8, 0.2, 0.15, 0.4, 0.4)], seg=16)
    for i in range(9):
        x = -3.8 + i * 0.95
        r = 1.95 * math.sqrt(max(0.05, 1 - (x / 5.0) ** 2))
        torus((x, 0, 0), (0, 90, 0), R=r, r=0.06, seg=40, mseg=8, scale=(1, 1, 0.42))
    for s in (1, -1):
        wing((-2.0, 1.5 * s, -0.1), 3.4 * s, 3.0, 1.1, 0.08, sweep=1.6, dihedral=-0.3)
        wing((-4.0, 0.4 * s, 0.2), 1.5 * s, 1.2, 0.4, 0.06, sweep=0.9, dihedral=0.7)


def xindi_primate():
    # A long central hull with four forward-raked arms ending in pods, braced by a mid-ship ring.
    loft([(5.2, 0.1, 0.1, 0, 0.4), (3.5, 0.6, 0.5, 0, 0.5), (-2.0, 0.75, 0.6, 0, 0.6), (-5.0, 0.5, 0.45, 0, 0.6)], seg=20)
    for a in (45, 135, 225, 315):
        d = Vector((0, math.cos(math.radians(a)), math.sin(math.radians(a))))
        arm = cyl((0, 0, 0), r1=0.14, r2=0.1, depth=2.6, seg=12)
        q = Vector((0, 0, 1)).rotation_difference(d + Vector((0.35, 0, 0)))
        arm.rotation_mode = "QUATERNION"; arm.rotation_quaternion = q
        arm.location = Vector((-0.8, 0, 0)) + d * 1.3
        pod = loft([(1.4, 0.05, 0.05, 0, 0.3), (0.6, 0.3, 0.3, 0, 0.5), (-1.2, 0.34, 0.34, 0, 0.6), (-1.6, 0.2, 0.2, 0, 0.5)], seg=14)
        pod.location = Vector((-0.4, 0, 0)) + d * 2.5
    torus((-0.8, 0, 0), (0, 90, 0), R=1.25, r=0.1, seg=48, mseg=8)
    greeble(1.4, (-1.5, 0, 0), 30, 23, (0.05, 0.12))


def xindi_weapon():
    # The Xindi superweapon: a huge armoured sphere-cone with three swept ribs, a forward emitter cone and lattice greebles.
    core = uvsphere((0, 0, 0), (3.2, 3.0, 3.0), seg=48, rings=24)
    cone = cyl((3.8, 0, 0), rot=(0, 90, 0), r1=2.0, r2=0.25, depth=3.4, seg=32)
    for i in range(3):
        a = math.radians(90 + 120 * i)
        rib = torus((0, 0, 0), (0, 0, 0), R=3.35, r=0.28, seg=48, mseg=10, arc=200)
        rib.rotation_euler = (a, 0, 0)
        fin = wing((-2.0, 0, 0), 4.6, 3.4, 1.2, 0.18, sweep=1.8)
        fin.rotation_euler = (a, 0, 0)
    torus((-1.0, 0, 0), (0, 90, 0), R=3.1, r=0.18, seg=48, mseg=8)
    greeble(5.6, (0, 0, 0), 60, 31, (0.03, 0.08))


def suliban_cell_ship():
    # The Suliban cell ship: a small flattened pod made of stacked, ridged segments with a blunt nose.
    for i in range(7):
        x = 2.4 - i * 0.8
        w = 1.0 * math.sqrt(max(0.15, 1 - ((x + 0.2) / 3.0) ** 2))
        uvsphere((x, 0, 0), (0.48, w, 0.42 * w), seg=24, rings=12)
    loft([(3.0, 0.05, 0.05, 0, 0.1), (2.2, 0.5, 0.3, 0, 0.2), (-2.6, 0.55, 0.3, 0, 0.3), (-3.2, 0.2, 0.15, 0, 0.2)], seg=16)
    for s in (1, -1):
        wing((-1.2, 0.8 * s, 0), 1.5 * s, 1.4, 0.5, 0.05, sweep=0.6, dihedral=-0.1)


def nausicaan_raider():
    # A brutal arrowhead: sharp prow, downswept armoured wings with blade tips, a raised command block and twin engines.
    loft([(5.0, 0.04, 0.04, 0, 0.6), (3.2, 0.5, 0.25, 0, 0.8), (0.0, 0.9, 0.42, 0.05, 0.85), (-3.5, 1.0, 0.45, 0.05, 0.9),
          (-4.6, 0.8, 0.35, 0.05, 0.9)], seg=16)
    box((-0.8, 0, 0.55), (1.2, 0.45, 0.22), bevel=0.05)
    for s in (1, -1):
        wing((-1.5, 0.8 * s, -0.05), 4.0 * s, 3.4, 0.6, 0.12, sweep=2.2, dihedral=-1.1)
        wing((-4.0, 3.6 * s, -1.05), 4.2 * s, 1.2, 0.2, 0.1, sweep=-1.4, dihedral=-0.6)
        cyl((-4.4, 0.55 * s, 0), rot=(0, 90, 0), r1=0.32, r2=0.26, depth=1.6, seg=16)
    greeble(1.6, (-1.5, 0, 0.3), 20, 41)


def ferengi_pod():
    # A small Ferengi raider (20 Nov): the Marauder's crescent shrunk to a pod - a bulbous rounded body, a forked
    # prow, two swept crescent wings curling forward and a pair of aft impulse pods.
    loft([(4.0, 0.1, 0.08, 0, 0.2), (3.0, 0.7, 0.35, 0.05, 0.3), (1.0, 1.25, 0.6, 0.1, 0.35), (-1.5, 1.2, 0.55, 0.05, 0.4),
          (-3.0, 0.7, 0.35, 0, 0.5), (-3.6, 0.2, 0.12, 0, 0.5)], seg=24)
    for s in (1, -1):
        wing((-0.8, 1.0 * s, 0.0), 3.6 * s, 3.0, 0.5, 0.14, sweep=-1.6, dihedral=0.25)
        uvsphere((1.6, 3.4 * s, 0.25), scale=(1.1, 0.28, 0.2))
        box((4.4, 0.45 * s, 0.0), (0.8, 0.14, 0.12), bevel=0.04)
        cyl((-3.2, 0.7 * s, -0.1), rot=(0, 90, 0), r1=0.3, r2=0.22, depth=1.1, seg=16)
    uvsphere((0.6, 0, 0.55), scale=(1.2, 0.6, 0.35))


def ferengi_scout():
    # A light Ferengi scout (20 Nov): a slim rounded hull under a single forward-curving crescent wing, a raised
    # bridge blister and twin aft impulse vents. (The print model tried first came in two misaligned halves.)
    loft([(4.5, 0.08, 0.06, 0, 0.2), (3.0, 0.55, 0.3, 0.05, 0.3), (0.5, 0.85, 0.45, 0.1, 0.35), (-2.5, 0.75, 0.4, 0.05, 0.45),
          (-4.0, 0.25, 0.15, 0, 0.5)], seg=24)
    for s in (1, -1):
        wing((-1.2, 0.7 * s, -0.05), 3.0 * s, 2.6, 0.4, 0.12, sweep=-2.2, dihedral=-0.3)
        cyl((-3.6, 0.45 * s, 0.0), rot=(0, 90, 0), r1=0.22, r2=0.16, depth=0.9, seg=16)
    uvsphere((1.2, 0, 0.45), scale=(0.9, 0.45, 0.3))


def bajoran_lightship():
    # The Bajoran solar-sail ship: a slim wooden-looking hull, a mast frame and three great fan sails.
    loft([(3.0, 0.05, 0.05, 0, 0.3), (2.2, 0.35, 0.3, 0, 0.6), (-2.2, 0.4, 0.35, 0, 0.7), (-3.0, 0.2, 0.2, 0, 0.5)], seg=14)
    for i, a in enumerate((90, 210, 330)):
        r = math.radians(a)
        d = Vector((0, math.cos(r), math.sin(r)))
        mast = cyl((0, 0, 0), r1=0.04, r2=0.03, depth=4.4, seg=8)
        mast.rotation_mode = "QUATERNION"; mast.rotation_quaternion = Vector((0, 0, 1)).rotation_difference(d)
        mast.location = Vector((0.2, 0, 0)) + d * 2.2
        # the sail: a broad swept fan from the hull out along the mast (built in XY, span +Y, then turned onto d)
        so = wing((0.0, 0.25, 0.0), 4.3, 3.6, 1.6, thick=0.03, sweep=1.4)
        so.rotation_mode = "QUATERNION"; so.rotation_quaternion = Vector((0, 1, 0)).rotation_difference(d)


def borg_scout():
    # A small Borg scout: an elongated cuboid hull with lattice greebles and a forward sensor block.
    box((0, 0, 0), (2.4, 1.0, 0.8))
    box((2.7, 0, 0), (0.4, 0.6, 0.5))
    greeble(2.0, (0, 0, 0), 120, 51, (0.03, 0.1))
    for x in (-1.6, 0.0, 1.6):
        torus((x, 0, 0), (0, 90, 0), R=1.15, r=0.05, seg=4, mseg=4, scale=(1, 1, 0.8))


def borg_outpost():
    # A Borg node: three interlocked cubes of lattice on a central spine.
    for i, (x, s) in enumerate(((0, 2.2), (2.6, 1.3), (-2.6, 1.3))):
        box((x, 0, 0), (s, s, s))
        greeble(2 * s, (x, 0, 0), int(60 * s), 61 + i, (0.03, 0.09))
    cyl((0, 0, 0), rot=(0, 90, 0), r1=0.5, r2=0.5, depth=8.0, seg=8)


def fed_wells():
    # The 29th-century Wells class: a sleek elongated arrowhead hull with integrated swept nacelles, no saucer neck.
    b = uvsphere((0, 0, 0), (5.0, 1.6, 0.42), seg=48, rings=20)
    loft([(4.6, 0.05, 0.05, 0.2, 0.2), (2.0, 0.55, 0.3, 0.32, 0.3), (-3.0, 0.7, 0.35, 0.3, 0.4), (-4.6, 0.3, 0.2, 0.2, 0.3)], seg=18)
    for s in (1, -1):
        n = loft([(1.5, 0.05, 0.05, 0, 0.2), (0.8, 0.32, 0.22, 0, 0.3), (-4.0, 0.36, 0.25, 0, 0.4), (-4.8, 0.25, 0.18, 0, 0.3)], seg=16)
        n.location = (-1.0, 1.9 * s, -0.25)
        wing((-1.8, 0.9 * s, -0.12), 1.85 * s, 2.0, 1.2, 0.1, sweep=0.5, dihedral=-0.12)


def fed_k7():
    # Deep Space Station K-7 (TOS "The Trouble with Tribbles"): a central core with a domed upper tower and a long lower spire,
    # a broad hub ring, and three long arms at 120 degrees, each angled slightly down to a cylindrical end module with domed
    # caps and a docking collar.
    cyl((0, 0, 0), r1=1.6, r2=1.6, depth=1.6, seg=48)                      # hub drum
    cyl((0, 0, 1.05), r1=1.6, r2=1.1, depth=0.5, seg=48)                   # upper shoulder
    cyl((0, 0, -1.05), r1=1.1, r2=1.6, depth=0.5, seg=48)                  # lower shoulder
    torus((0, 0, 0), R=1.75, r=0.22, seg=64, mseg=12)                      # window band
    cyl((0, 0, 2.1), r1=0.55, r2=0.55, depth=1.8, seg=32)                  # upper tower
    cyl((0, 0, 3.05), r1=0.95, r2=0.95, depth=0.18, seg=40)                # control deck
    uvsphere((0, 0, 3.2), (0.8, 0.8, 0.45), seg=32, rings=16)              # dome
    cyl((0, 0, 3.95), r1=0.04, r2=0.02, depth=0.9, seg=8)                  # antenna mast
    cyl((0, 0, -2.6), r1=0.18, r2=0.7, depth=2.6, seg=32)                  # lower spire
    cyl((0, 0, -4.2), r1=0.05, r2=0.18, depth=0.8, seg=16)                 # spire tip
    torus((0, 0, -2.0), R=0.9, r=0.1, seg=40, mseg=8)                      # spire ring
    for a in (0, 120, 240):
        r = math.radians(a)
        c, s = math.cos(r), math.sin(r)
        arm = cyl((0, 0, 0), r1=0.42, r2=0.3, depth=3.2, seg=20)
        arm.rotation_euler = (0, math.radians(90 + 6), r)
        arm.location = (3.2 * c, 3.2 * s, -0.15)
        brace = box((0, 0, 0), (1.6, 0.08, 0.35))
        brace.rotation_euler = (0, math.radians(6), r)
        brace.location = (3.0 * c, 3.0 * s, 0.2)
        mx, my, mz = 5.3 * c, 5.3 * s, -0.4
        cyl((mx, my, mz), r1=0.75, r2=0.75, depth=1.5, seg=32)            # end module
        uvsphere((mx, my, mz + 0.75), (0.75, 0.75, 0.35), seg=24, rings=12)
        uvsphere((mx, my, mz - 0.75), (0.75, 0.75, 0.3), seg=24, rings=12)
        torus((mx, my, mz), R=0.85, r=0.1, seg=32, mseg=8)                 # docking collar
        cyl((mx, my, mz + 1.35), r1=0.03, r2=0.02, depth=0.5, seg=6)      # beacon mast


def fer_station(big):
    # Ferengi station: a tiered central dome with the Ferengi horseshoe arms (a starbase has three of them and docking rings).
    uvsphere((0, 0, 0.3), (2.2, 2.2, 1.1), seg=40, rings=20)
    cyl((0, 0, -0.8), r1=2.8, r2=2.0, depth=1.0, seg=40)
    cyl((0, 0, -2.0), r1=0.7, r2=1.4, depth=1.6, seg=24)
    arms = (0, 120, 240) if big else (0, 180)
    for a in arms:
        t = torus((0, 0, 0), (0, 0, a), R=4.2 if big else 3.4, r=0.45, seg=32, mseg=12, arc=110, scale=(1, 1, 0.5))
        t.location = (0, 0, -0.6)
        for side in (-1, 1):
            ang = math.radians(a + side * 55)
            R = 4.2 if big else 3.4
            uvsphere((R * math.cos(ang), R * math.sin(ang), -0.6), (0.7, 0.7, 0.5), seg=20, rings=10)
        strut = box((0, 0, 0), (1.6 if big else 1.3, 0.18, 0.12))
        strut.rotation_euler = (0, 0, math.radians(a)); strut.location = (2.2 * math.cos(math.radians(a)), 2.2 * math.sin(math.radians(a)), -0.6)
    if big:
        torus((0, 0, -0.8), R=6.0, r=0.2, seg=64, mseg=10)
        for a in range(0, 360, 60):
            r = math.radians(a + 30)
            box((4.6 * math.cos(r), 4.6 * math.sin(r), -0.8), (1.2, 0.1, 0.08), rot=(0, 0, a + 30))
    greeble(4.0, (0, 0, -0.4), 50, 71, (0.03, 0.08))


def breen_station(big):
    # Breen station: a squat armoured hub with downswept angular pylons (a starbase has six and a ring of hangar blocks).
    loft([(0, 2.4, 2.4, 0.0, 0.2)], seg=6) if False else None
    cyl((0, 0, 0), r1=2.6, r2=1.6, depth=1.6, seg=6)
    cyl((0, 0, -1.4), r1=1.2, r2=2.6, depth=1.2, seg=6)
    cyl((0, 0, 1.2), r1=0.4, r2=1.0, depth=1.0, seg=6)
    n = 6 if big else 3
    for i in range(n):
        a = 360.0 * i / n
        p = wing((0, 1.6, 0), 6.0 if big else 4.6, 2.0, 0.5, 0.25, sweep=-0.6, dihedral=-1.6)
        p.rotation_euler = (0, 0, math.radians(a - 90))
    if big:
        for i in range(6):
            r = math.radians(30 + 60 * i)
            box((4.0 * math.cos(r), 4.0 * math.sin(r), -0.9), (0.8, 0.6, 0.5), rot=(0, 0, 30 + 60 * i), bevel=0.08)
    greeble(4.0, (0, 0, 0), 40, 81)


def threat_crystalline():
    # The Crystalline Entity: a vast snowflake of long crystal spines radiating from a dense core.
    rnd = random.Random(91)
    uvsphere((0, 0, 0), (1.2, 1.2, 1.2), seg=12, rings=6)
    for i in range(40):
        theta = rnd.uniform(0, 2 * math.pi)
        phi = rnd.gauss(0, 0.35)
        d = Vector((math.cos(theta) * math.cos(phi), math.sin(theta) * math.cos(phi), math.sin(phi)))
        L = rnd.uniform(3.0, 5.5)
        sp = cyl((0, 0, 0), r1=rnd.uniform(0.18, 0.32), r2=0.0, depth=L, seg=6)
        sp.rotation_mode = "QUATERNION"; sp.rotation_quaternion = Vector((0, 0, 1)).rotation_difference(d)
        sp.location = d * (L / 2 + 0.6)
        for k in range(2):
            b = d.orthogonal().normalized()
            b.rotate(Matrix.Rotation(rnd.uniform(0, 6.28), 3, d))
            l2 = L * rnd.uniform(0.25, 0.4)
            br = cyl((0, 0, 0), r1=0.1, r2=0.0, depth=l2, seg=6)
            dd = (d + b * 0.9).normalized()
            br.rotation_mode = "QUATERNION"; br.rotation_quaternion = Vector((0, 0, 1)).rotation_difference(dd)
            br.location = d * rnd.uniform(1.5, L * 0.8) + dd * (l2 / 2)


def threat_calamarain():
    # The Calamarain: a drifting cloud of ionised gas, modelled as a lumpy cluster of soft lobes.
    rnd = random.Random(101)
    for i in range(14):
        p = Vector((rnd.uniform(-3.5, 3.5), rnd.uniform(-2.0, 2.0), rnd.uniform(-1.2, 1.2)))
        s = rnd.uniform(1.0, 2.0)
        displace(uvsphere(tuple(p), (s * 1.3, s, s * 0.8), seg=16, rings=8), strength=0.45 * s, size=0.6, seed=i)


def threat_gomtuu():
    # Gomtuu, the living ship: a long organic shell, ribbed and lumpy, with a split bow.
    body = loft([(5.0, 0.2, 0.2, 0, 0.0), (3.5, 1.2, 1.0, 0, 0.0), (0.0, 1.8, 1.4, 0, 0.0), (-3.5, 1.3, 1.0, 0, 0.0), (-5.2, 0.4, 0.3, 0, 0.0)], seg=24)
    displace(body, 0.35, 0.7, 7)
    for i in range(8):
        torus((3.2 - i * 0.95, 0, 0), (0, 90, 0), R=1.6 - abs(i - 3.5) * 0.15, r=0.16, seg=32, mseg=8, scale=(1, 0.95, 0.78))
    for s in (1, -1):
        h = loft([(1.6, 0.05, 0.05, 0, 0.0), (0.4, 0.35, 0.3, 0, 0.0), (-0.8, 0.3, 0.25, 0, 0.0)], seg=12)
        h.location = (5.2, 0.6 * s, 0.1)


def threat_echo_papa():
    # Echo Papa 607: the Arsenal of Freedom drone, a small faceted pod with stub wings, a sensor ring and a weapon prong.
    loft([(1.6, 0.1, 0.1, 0, 0.5), (0.8, 0.55, 0.45, 0, 0.6), (-0.8, 0.6, 0.5, 0, 0.6), (-1.5, 0.35, 0.3, 0, 0.5)], seg=8)
    torus((0.0, 0, 0), (0, 90, 0), R=0.8, r=0.07, seg=24, mseg=6)
    for s in (1, -1):
        wing((-0.3, 0.5 * s, 0), 1.3 * s, 0.9, 0.35, 0.06, sweep=0.3, dihedral=-0.15)
    cyl((2.0, 0, -0.2), rot=(0, 90, 0), r1=0.06, r2=0.03, depth=1.0, seg=8)


def threat_edo_guardian():
    # The Edo god: a floating geometric construct of nested, rotated polyhedra joined by spars.
    for i, (s, r) in enumerate(((2.6, (0, 0, 0)), (2.1, (45, 0, 45)), (1.6, (0, 45, 30)), (1.1, (30, 60, 0)))):
        b = box((0, 0, 0), (s, s, s), rot=r)
        w = b.modifiers.new("wf", "WIREFRAME"); w.thickness = 0.12
    uvsphere((0, 0, 0), (0.7, 0.7, 0.7), seg=8, rings=4)


def threat_husnock():
    # A Husnock warship: a wide, flat, heavily armoured hull with forward claws and a raised dorsal citadel.
    loft([(4.6, 0.3, 0.12, 0, 0.8), (3.0, 1.8, 0.35, 0, 0.9), (-2.0, 2.6, 0.5, 0, 0.9), (-4.4, 2.2, 0.45, 0, 0.9)], seg=16)
    box((-0.5, 0, 0.6), (2.2, 0.9, 0.35), bevel=0.06)
    box((-1.6, 0, 1.05), (0.9, 0.5, 0.25), bevel=0.05)
    for s in (1, -1):
        c = loft([(2.4, 0.05, 0.05, 0, 0.6), (1.2, 0.3, 0.16, 0, 0.8), (-0.6, 0.35, 0.18, 0, 0.8)], seg=10)
        c.location = (4.0, 1.4 * s, -0.05); c.rotation_euler = (0, 0, math.radians(-10 * s))
        cyl((-4.6, 1.5 * s, 0), rot=(0, 90, 0), r1=0.42, r2=0.36, depth=1.0, seg=16)
    greeble(3.0, (-1.0, 0, 0.2), 40, 111)


def threat_chodak():
    # The Chodak dreadnought: a long, angular, layered hull with stacked gun decks and a heavy aft drive section.
    loft([(5.4, 0.2, 0.2, 0, 0.9), (3.6, 0.9, 0.6, 0, 0.95), (-2.0, 1.2, 0.8, 0, 0.95), (-5.0, 1.5, 1.0, 0, 0.95)], seg=12)
    for z in (0.95, -0.95):
        box((0.0, 0, z), (3.6, 0.9, 0.16), bevel=0.05)
    for s in (1, -1):
        box((-1.0, 1.3 * s, 0), (3.0, 0.22, 0.6), bevel=0.05)
        cyl((-5.4, 0.9 * s, 0.4), rot=(0, 90, 0), r1=0.5, r2=0.42, depth=1.2, seg=12)
        cyl((-5.4, 0.9 * s, -0.4), rot=(0, 90, 0), r1=0.5, r2=0.42, depth=1.2, seg=12)
    greeble(3.0, (0.0, 0, 0), 60, 121)


def threat_tarellian():
    # A Tarellian plague ship (TNG "Haven"): an old, slow cylinder with banded rings, a blunt bow and a single aft drive.
    cyl((0, 0, 0), rot=(0, 90, 0), r1=1.0, r2=1.0, depth=7.0, seg=24)
    uvsphere((3.5, 0, 0), (0.8, 1.0, 1.0), seg=24, rings=12)
    cyl((-4.0, 0, 0), rot=(0, 90, 0), r1=0.6, r2=1.0, depth=1.2, seg=24)
    for x in (-2.4, -0.8, 0.8, 2.4):
        torus((x, 0, 0), (0, 90, 0), R=1.12, r=0.12, seg=32, mseg=8)
    for s in (1, -1):
        box((0, 1.2 * s, 0), (1.6, 0.18, 0.5), bevel=0.04)


MODELLED = {
    "ship.xindi.reptilian": xindi_reptilian, "ship.xindi.insectoid": xindi_insectoid, "ship.xindi.aquatic": xindi_aquatic,
    "ship.xindi.primate": xindi_primate, "ship.xindi.weapon": xindi_weapon, "ship.sul.cell_ship": suliban_cell_ship,
    "ship.nau.raider": nausicaan_raider, "ship.fer.pod": ferengi_pod, "ship.fer.scout": ferengi_scout, "ship.baj.lightship": bajoran_lightship, "ship.borg.scout": borg_scout,
    "ship.borg.outpost": borg_outpost, "ship.fed.wells": fed_wells, "ship.fed.k7": fed_k7,
    "ship.fer.outpost": lambda: fer_station(False), "ship.fer.starbase": lambda: fer_station(True),
    "ship.breen.outpost": lambda: breen_station(False), "ship.breen.starbase": lambda: breen_station(True),
    "ship.threat.crystalline": threat_crystalline, "ship.threat.calamarain": threat_calamarain,
    "ship.threat.gomtuu": threat_gomtuu, "ship.threat.echo_papa": threat_echo_papa,
    "ship.threat.edo_guardian": threat_edo_guardian, "ship.threat.husnock": threat_husnock,
    "ship.threat.chodak": threat_chodak, "ship.threat.tarellian": threat_tarellian,
}


# ---------------------------------------------------------------------------------------------------------------------
# Kitbash: recombine a Bridge Commander model (materials and UVs kept)
# ---------------------------------------------------------------------------------------------------------------------
def import_bc(bc_id):
    bpy.ops.import_scene.fbx(filepath=os.path.join(BC, bc_id, "SM_%s.fbx" % bc_id))
    meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
    for o in list(bpy.context.scene.objects):
        if o.type != "MESH":
            for c in o.children:
                c.parent = None
    bpy.ops.object.select_all(action="DESELECT")
    for o in meshes:
        o.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    bpy.ops.object.parent_clear(type="CLEAR_KEEP_TRANSFORM")
    for o in list(bpy.context.scene.objects):
        if o.type != "MESH":
            bpy.data.objects.remove(o, do_unlink=True)
    if len(meshes) > 1:
        bpy.ops.object.join()
    ob = bpy.context.view_layer.objects.active
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    # Centre on bounds; record the length (BC frame: nose +X, top +Z as built by the Starhelm pipeline).
    bb = [ob.matrix_world @ Vector(c) for c in ob.bound_box]
    lo = Vector([min(v[i] for v in bb) for i in range(3)]); hi = Vector([max(v[i] for v in bb) for i in range(3)])
    ob.data.transform(Matrix.Translation(-(lo + hi) / 2))
    ob["L"] = max(hi - lo)
    return ob


def dup(ob, loc=(0, 0, 0), rot=(0, 0, 0), scale=(1, 1, 1)):
    n = ob.copy(); n.data = ob.data.copy()
    bpy.context.scene.collection.objects.link(n)
    n.location = Vector(loc) * ob["L"]
    n.rotation_euler = Euler([math.radians(r) for r in rot])
    n.scale = Vector(scale)
    return n


def cut(ob, axis, keep_above, at):
    """Bisects ob on the plane axis = at (fraction of its length) and keeps one side, filling the cut."""
    bpy.context.view_layer.objects.active = ob
    bpy.ops.object.select_all(action="DESELECT"); ob.select_set(True)
    bpy.ops.object.mode_set(mode="EDIT"); bpy.ops.mesh.select_all(action="SELECT")
    no = Vector([1.0 if i == axis else 0.0 for i in range(3)])
    co = no * (at * ob["L"])
    bpy.ops.mesh.bisect(plane_co=co, plane_no=no, use_fill=True, clear_inner=keep_above, clear_outer=not keep_above)
    bpy.ops.object.mode_set(mode="OBJECT")
    return ob


def finish_fbx(path):
    obs = [o for o in bpy.context.scene.objects if o.type == "MESH"]
    bpy.ops.object.select_all(action="DESELECT")
    for o in obs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = obs[0]
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    if len(obs) > 1:
        bpy.ops.object.join()
    hull = bpy.context.view_layer.objects.active
    os.makedirs(os.path.dirname(path), exist_ok=True)
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, apply_unit_scale=True, bake_space_transform=False,
                             axis_forward="-Y", axis_up="Z", path_mode="STRIP")
    tris = sum(len(p.vertices) - 2 for p in hull.data.polygons)
    print("F4BUILD", os.path.basename(path), "tris", tris)


def kb_fer_bronta():
    # Scout: the Marauder's forward hull alone, shortened, with a small aft drive pod from a Merchantman.
    m = import_bc("Marauder")
    cut(m, 0, True, -0.05)
    m.scale = (0.9, 0.75, 0.9)
    c = import_bc("Merchantman"); c.scale = (0.35, 0.35, 0.35); c.location = (-0.32 * m["L"], 0, 0)


def kb_fer_ngort():
    # Destroyer: a narrow Marauder with its wings pulled in and a second, smaller hull slung below.
    m = import_bc("Marauder"); m.scale = (1.0, 0.7, 1.0)
    d = dup(m, (-0.05, 0, -0.16), (0, 0, 0), (0.6, 0.5, 0.6))


def kb_fer_goront():
    # Cruiser: a Marauder flanked by two Merchantman hulls as outboard pods.
    m = import_bc("Marauder")
    L = m["L"]
    for s in (1, -1):
        c = import_bc("Merchantman"); c.scale = (0.55, 0.55, 0.55); c.location = (-0.1 * L, 0.42 * s * L, -0.04 * L)


def kb_fer_ooron():
    # Heavy cruiser: two Marauder hulls stacked, the upper one reversed in sweep and smaller.
    m = import_bc("Marauder")
    dup(m, (0.08, 0, 0.14), (0, 0, 0), (0.7, 0.85, 0.7))


def kb_fer_tokorn():
    # Battleship: a Merchantman spine with a Marauder forward and a Marauder wing pair aft.
    c = import_bc("Merchantman"); c.scale = (1.2, 1.0, 1.0)
    L = c["L"]
    m = import_bc("Marauder"); m.scale = (0.7, 0.7, 0.7); m.location = (0.45 * L, 0, 0.02 * L)
    cut(dup(m, (0, 0, 0)), 0, False, 0.05).location = (-0.35 * L, 0, 0)


def kb_fer_glantor():
    # Troop ship: two Merchantman hulls side by side joined amidships.
    c = import_bc("Merchantman"); L = c["L"]
    c.location = (0, 0.2 * L, 0)
    dup(c, (0, -0.2, 0))
    b = dup(c, (0.05, 0, 0.02), (0, 0, 0), (0.5, 0.6, 0.4))


def kb_car_toran():
    # Troop ship: the Cardassian freighter with a second cargo spine slung below and a raised command block.
    c = import_bc("CardFreighter__km")
    dup(c, (-0.05, 0, -0.12), (0, 0, 0), (0.8, 0.8, 0.8))


def kb_car_ranol():
    # Colony ship: the freighter shortened, with two outboard cargo modules.
    c = import_bc("CardFreighter")
    cut(c, 0, True, -0.2)
    for s in (1, -1):
        d = dup(c, (-0.05, 0.16 * s, 0), (0, 0, 0), (0.55, 0.6, 0.6))


KITBASH = {
    "ship.fer.bronta": kb_fer_bronta, "ship.fer.ngort": kb_fer_ngort, "ship.fer.goront": kb_fer_goront,
    "ship.fer.ooron": kb_fer_ooron, "ship.fer.tokorn": kb_fer_tokorn, "ship.fer.glantor": kb_fer_glantor,
    "ship.car.toran": kb_car_toran, "ship.car.ranol": kb_car_ranol,
}


def main():
    for key, fn in MODELLED.items():
        if ONLY and key not in ONLY:
            continue
        reset(); fn()
        finish(os.path.join(OUT, key, "SM_%s.obj" % key.split(".")[-1]))
    for key, fn in KITBASH.items():
        if ONLY and key not in ONLY:
            continue
        reset(); fn()
        finish_fbx(os.path.join(OUT, key, "SM_%s.fbx" % key.split(".")[-1]))


main()
