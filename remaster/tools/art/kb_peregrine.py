"""The 30 m Peregrine-class fighter (original model, built from primitives in Blender; +X nose, +Y port, +Z up).

Canon: the Peregrine class of DS9 'Valiant' and VOY 'Caretaker' is a small Starfleet attack fighter: a flat wedge hull
that narrows to a pointed nose, a raised canopy, two short nacelles on the aft quarters. Dimensions here: 30 m long,
about 14 m across the nacelles, 4 m tall. Paint is a vertex colour "Col" (cool grey hull, darker belly, dark canopy,
blue bussard caps, amber exhausts) under the faction plating, like the generated ships.
"""
import math

import bmesh
import bpy
import numpy as np
from mathutils import Matrix, Vector

COLORS = []   # per-vertex linear RGB, in vertex order, filled while building
HULL = (0.50, 0.52, 0.56)
BELLY = (0.31, 0.33, 0.37)
NAC = (0.42, 0.44, 0.49)
CANOPY = (0.012, 0.02, 0.04)
BUSSARD = (0.04, 0.22, 0.95)
EXHAUST = (0.95, 0.42, 0.10)
DARK = (0.10, 0.11, 0.13)

PEREGRINE_SOCKETS = [('WPN_Torp_01', (14.2, 0.0, 0.0)), ('WPN_Beam_01', (11.5, 1.8, 0.35)), ('WPN_Beam_02', (11.5, -1.8, 0.35)),
                     ('ENG_01', (-15.2, 7.3, 0.3)), ('ENG_02', (-15.2, -7.3, 0.3))]


def _ring(bm, x, w, ht, hb, n=3.0, m=28):
    vs = []
    for k in range(m):
        t = 2 * math.pi * k / m
        c, s = math.cos(t), math.sin(t)
        y = w * math.copysign(abs(c) ** (2 / n), c)
        z = (ht if s > 0 else hb) * math.copysign(abs(s) ** (2 / n), s)
        vs.append(bm.verts.new((x, y, z)))
    return vs


def _bridge(bm, a, b):
    m = len(a)
    for k in range(m):
        bm.faces.new((a[k], a[(k + 1) % m], b[(k + 1) % m], b[k]))


def _tag(bm, start, rgb):
    for v in list(bm.verts)[start:]:
        COLORS.append(rgb)


def peregrine_object():
    COLORS.clear()
    bm = bmesh.new()

    def mark(fn, rgb_fn):
        n0 = len(bm.verts)
        fn()
        for i, v in enumerate(list(bm.verts)[n0:]):
            COLORS.append(rgb_fn(v.co))
    # ---- main wedge: stations from nose (+15) to tail (-15)
    xs = [15.0, 14.2, 12.5, 10.0, 7.0, 4.0, 0.0, -4.0, -8.0, -11.5, -14.0, -15.0]
    rings = []

    def hull():
        for x in xs:
            u = (15.0 - x) / 30.0
            if x > 4:
                w = 6.4 * ((15.0 - x) / 11.0) ** 0.85
            elif x > -12:
                w = 6.4
            else:
                w = 6.4 - (x + 12) * -1.0 * 0.6
            w = max(w, 0.08)
            ht = 1.7 * min(1.0, (15.0 - x) / 9.0) ** 0.6 + (0.25 if -10 < x < 6 else 0.0)
            hb = 0.95 * min(1.0, (15.0 - x) / 8.0) ** 0.5
            ht, hb = max(ht, 0.05), max(hb, 0.05)
            rings.append(_ring(bm, x, w, ht, hb))
        for a, b in zip(rings[:-1], rings[1:]):
            _bridge(bm, a, b)
        bm.faces.new(rings[0][::-1])
        bm.faces.new(rings[-1])
    mark(hull, lambda co: BELLY if co.z < -0.15 else HULL)
    # ---- canopy
    def canopy():
        bmesh.ops.create_uvsphere(bm, u_segments=20, v_segments=10, radius=1.0, matrix=Matrix.Translation((5.6, 0, 1.55)) @ Matrix.Diagonal((3.6, 1.35, 0.95, 1)))
    mark(canopy, lambda co: CANOPY)
    # ---- dorsal spine and keel
    def spine():
        bmesh.ops.create_cube(bm, size=1.0, matrix=Matrix.Translation((-5.5, 0, 2.15)) @ Matrix.Diagonal((11.0, 1.0, 0.55, 1)))
        bmesh.ops.create_cube(bm, size=1.0, matrix=Matrix.Translation((-1.0, 0, -1.05)) @ Matrix.Diagonal((16.0, 1.1, 0.4, 1)))
    mark(spine, lambda co: HULL if co.z > 0 else BELLY)
    # ---- wing strakes along both edges (raised leading-edge plates)
    def strakes():
        for sgn in (1, -1):
            bmesh.ops.create_cube(bm, size=1.0, matrix=Matrix.Translation((-2.0, sgn * 5.5, 0.15)) @ Matrix.Rotation(sgn * 0.0, 4, 'Z') @ Matrix.Diagonal((13.0, 1.5, 0.45, 1)))
    mark(strakes, lambda co: DARK if abs(co.z - 0.15) < 0.1 else HULL)
    # ---- nacelles: pylon, tube, bussard cap, exhaust
    def nacelles():
        for sgn in (1, -1):
            m = Matrix.Translation((-8.0, sgn * 7.2, 0.45)) @ Matrix.Rotation(math.pi / 2, 4, 'Y')
            bmesh.ops.create_cone(bm, cap_ends=True, cap_tris=False, segments=20, radius1=1.05, radius2=1.05, depth=13.0, matrix=m)
    mark(nacelles, lambda co: NAC)
    def pylons():
        for sgn in (1, -1):
            bmesh.ops.create_cube(bm, size=1.0, matrix=Matrix.Translation((-8.0, sgn * 5.9, 0.2)) @ Matrix.Diagonal((6.0, 2.0, 0.35, 1)))
    mark(pylons, lambda co: HULL)
    def caps():
        for sgn in (1, -1):
            bmesh.ops.create_uvsphere(bm, u_segments=18, v_segments=9, radius=1.0, matrix=Matrix.Translation((-1.2, sgn * 7.2, 0.45)) @ Matrix.Diagonal((1.8, 1.0, 1.0, 1)))
    mark(caps, lambda co: BUSSARD)
    def exhausts():
        for sgn in (1, -1):
            m = Matrix.Translation((-14.6, sgn * 7.2, 0.45)) @ Matrix.Rotation(math.pi / 2, 4, 'Y')
            bmesh.ops.create_cone(bm, cap_ends=True, cap_tris=False, segments=18, radius1=0.85, radius2=0.7, depth=0.9, matrix=m)
        bmesh.ops.create_cone(bm, cap_ends=True, cap_tris=False, segments=18, radius1=1.4, radius2=1.1, depth=0.8,
                              matrix=Matrix.Translation((-15.2, 0, 0.3)) @ Matrix.Rotation(math.pi / 2, 4, 'Y'))
    mark(exhausts, lambda co: EXHAUST)
    # ---- weapon hardpoints (phaser banks) and the torpedo tube
    def guns():
        for sgn in (1, -1):
            m = Matrix.Translation((10.5, sgn * 1.8, 0.35)) @ Matrix.Rotation(math.pi / 2, 4, 'Y')
            bmesh.ops.create_cone(bm, cap_ends=True, cap_tris=False, segments=10, radius1=0.28, radius2=0.18, depth=4.0, matrix=m)
        bmesh.ops.create_cone(bm, cap_ends=True, cap_tris=False, segments=12, radius1=0.4, radius2=0.35, depth=1.2,
                              matrix=Matrix.Translation((14.4, 0, 0.0)) @ Matrix.Rotation(math.pi / 2, 4, 'Y'))
    mark(guns, lambda co: DARK)
    # ---- rear fins
    def fins():
        for sgn in (1, -1):
            bmesh.ops.create_cube(bm, size=1.0, matrix=Matrix.Translation((-11.5, sgn * 3.0, 1.5)) @ Matrix.Rotation(sgn * 0.35, 4, 'X') @ Matrix.Diagonal((4.0, 0.22, 2.2, 1)))
    mark(fins, lambda co: HULL)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    me = bpy.data.meshes.new('SM_peregrine')
    bm.to_mesh(me)
    bm.free()
    obj = bpy.data.objects.new('SM_peregrine', me)
    bpy.context.scene.collection.objects.link(obj)
    assert len(COLORS) == len(me.vertices), (len(COLORS), len(me.vertices))
    return obj


def peregrine_paint(hull):
    me = hull.data
    for name in [a.name for a in me.color_attributes]:
        me.color_attributes.remove(me.color_attributes[name])
    attr = me.color_attributes.new('Col', 'FLOAT_COLOR', 'POINT')
    flat = np.array([[c[0], c[1], c[2], 1.0] for c in COLORS], np.float32).ravel()
    attr.data.foreach_set('color', flat)
    me.color_attributes.active_color = attr
    bpy.context.view_layer.objects.active = hull
    hull.select_set(True)
    bpy.ops.geometry.color_attribute_convert(domain='CORNER', data_type='BYTE_COLOR')
    me.color_attributes.active_color = me.color_attributes['Col']
    me.color_attributes.render_color_index = me.color_attributes.active_color_index
    me.materials.clear()
    me.materials.append(bpy.data.materials.new('M00'))
