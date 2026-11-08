"""Kitbash helpers (Blender 5.x, bmesh) for build_art2_ships.py. Hull frame: +X nose, +Y port, +Z up, metres.

Edits work on fractions of the hull's bounding box (fx along the length, fy across, fz up) so a recipe reads the same on
any base hull. New geometry takes the base hull's own paint: its UVs are pinned to the plainest patch of the main slot's
base-colour map, so primitives pick up the hull colour, roughness and panel normal of the real texture.
"""
import math

import bmesh
import bpy
import numpy as np
from mathutils import Matrix, Vector


class Kit:
    def __init__(self, obj, main_slot, plain_uv):
        self.obj = obj
        self.bm = bmesh.new()
        self.bm.from_mesh(obj.data)
        self.main_slot = main_slot
        self.plain_uv = plain_uv
        self.uv = self.bm.loops.layers.uv.verify()
        self.refresh()

    # -- bounds ---------------------------------------------------------------------------------------------------
    def refresh(self):
        co = np.array([v.co[:] for v in self.bm.verts])
        self.lo, self.hi = co.min(0), co.max(0)
        self.ext = np.maximum(self.hi - self.lo, 1e-6)

    def f(self, v):
        return (np.array(v.co[:]) - self.lo) / self.ext

    def box(self, x=(0, 1), y=(0, 1), z=(0, 1), ay=None):
        """Vertices inside a fractional box. ay=(a,b): fraction of |y| from the centreline instead of y."""
        out = []
        for v in self.bm.verts:
            fx, fy, fz = self.f(v)
            if not (x[0] <= fx <= x[1] and z[0] <= fz <= z[1]):
                continue
            if ay is not None:
                a = abs(v.co.y) / (self.ext[1] * 0.5)
                if not (ay[0] <= a <= ay[1]):
                    continue
            elif not (y[0] <= fy <= y[1]):
                continue
            out.append(v)
        return out

    def at(self, fx, fy, fz):
        return Vector(self.lo + self.ext * np.array([fx, fy, fz]))

    # -- transforms -----------------------------------------------------------------------------------------------
    def move(self, verts, d):
        for v in verts:
            v.co += Vector(d)

    def scale(self, verts, pivot, s):
        s = Vector(s) if hasattr(s, '__len__') else Vector((s, s, s))
        for v in verts:
            r = v.co - Vector(pivot)
            v.co = Vector(pivot) + Vector((r.x * s.x, r.y * s.y, r.z * s.z))

    def warp(self, fn):
        """fn(co: Vector, f: ndarray) -> new Vector, over every vertex (whole-hull shaping with smooth falloffs)."""
        for v in self.bm.verts:
            v.co = fn(v.co.copy(), self.f(v))

    def stretch(self, sx=1.0, sy=1.0, sz=1.0):
        for v in self.bm.verts:
            v.co = Vector((v.co.x * sx, v.co.y * sy, v.co.z * sz))
        self.refresh()

    def delete(self, verts):
        bmesh.ops.delete(self.bm, geom=list(verts), context='VERTS')
        self.refresh()

    def dup(self, verts, matrix=None, mirror_y=False):
        """Duplicate the faces fully inside `verts`, transformed by matrix (about world origin); mirror_y flips Y."""
        vs = set(verts)
        faces = [fc for fc in self.bm.faces if all(v in vs for v in fc.verts)]
        if not faces:
            return []
        ret = bmesh.ops.duplicate(self.bm, geom=faces)
        newv = [g for g in ret['geom'] if isinstance(g, bmesh.types.BMVert)]
        newf = [g for g in ret['geom'] if isinstance(g, bmesh.types.BMFace)]
        m = matrix or Matrix.Identity(4)
        if mirror_y:
            m = Matrix.Diagonal((1, -1, 1, 1)) @ m
        bmesh.ops.transform(self.bm, matrix=m, verts=newv)
        if mirror_y or m.determinant() < 0:
            bmesh.ops.reverse_faces(self.bm, faces=newf)
        return newv

    # -- primitives (pinned to the plain patch of the main slot) ----------------------------------------------------
    def _paint(self, geom_faces, slot=None):
        u, v = self.plain_uv
        for fc in geom_faces:
            fc.material_index = self.main_slot if slot is None else slot
            for i, lp in enumerate(fc.loops):
                lp[self.uv].uv = (u + 0.002 * (i % 2), v + 0.002 * (i // 2 % 2))

    def box_prim(self, c, size, rot=(0, 0, 0), slot=None):
        m = Matrix.Translation(Vector(c)) @ Matrix.Rotation(rot[2], 4, 'Z') @ Matrix.Rotation(rot[1], 4, 'Y') @ Matrix.Rotation(rot[0], 4, 'X') \
            @ Matrix.Diagonal((size[0], size[1], size[2], 1))
        ret = bmesh.ops.create_cube(self.bm, size=1.0, matrix=m)
        fcs = list({lp.face for v in ret['verts'] for lp in v.link_loops})
        self._paint(fcs, slot)
        return ret['verts']

    def cyl(self, c, radius, length, axis='X', r2=None, segs=20, slot=None, caps=True):
        """Cylinder or cone frustum centred on c along `axis`; r2 = radius at the +axis end (default radius)."""
        r2 = radius if r2 is None else r2
        rot = {'X': Matrix.Rotation(math.pi / 2, 4, 'Y'), 'Y': Matrix.Rotation(-math.pi / 2, 4, 'X'), 'Z': Matrix.Identity(4)}[axis]
        m = Matrix.Translation(Vector(c)) @ rot
        ret = bmesh.ops.create_cone(self.bm, cap_ends=caps, cap_tris=False, segments=segs, radius1=radius, radius2=r2, depth=length, matrix=m)
        fcs = list({lp.face for v in ret['verts'] for lp in v.link_loops})
        self._paint(fcs, slot)
        return ret['verts']

    def sphere(self, c, radius, squash=(1, 1, 1), segs=16, slot=None):
        m = Matrix.Translation(Vector(c)) @ Matrix.Diagonal((squash[0], squash[1], squash[2], 1))
        ret = bmesh.ops.create_uvsphere(self.bm, u_segments=segs, v_segments=max(6, segs // 2), radius=radius, matrix=m)
        fcs = list({lp.face for v in ret['verts'] for lp in v.link_loops})
        self._paint(fcs, slot)
        return ret['verts']

    def finish(self):
        bmesh.ops.recalc_face_normals(self.bm, faces=self.bm.faces[:])
        self.bm.to_mesh(self.obj.data)
        self.bm.free()
        self.obj.data.update()


def plain_patch(image):
    """(u, v) of the plainest, most typical patch of a base-colour image: low local variance, close to the mean colour."""
    w, h = image.size
    px = np.empty(w * h * 4, np.float32)
    image.pixels.foreach_get(px)
    a = px.reshape(h, w, 4)[..., :3]
    s = 64
    a = a[: (h // s) * s, : (w // s) * s].reshape(s, h // s, s, w // s, 3)
    blk = a.mean((1, 3))
    var = a.var((1, 3)).sum(-1)
    lum = blk.mean(-1)
    mean = blk[(lum > 0.08) & (lum < 0.92)].mean(0)
    mlum = float(mean.mean())
    score = var / (var.mean() + 1e-9) + 2.0 * np.abs(blk - mean).sum(-1) + 6.0 * np.abs(lum - 0.8 * mlum) + 3.0 * ((lum < 0.1) | (lum > 0.9))
    j, i = np.unravel_index(np.argmin(score[1:-1, 1:-1]), (s - 2, s - 2))
    return ((i + 1.5) / s, (j + 1.5) / s)
