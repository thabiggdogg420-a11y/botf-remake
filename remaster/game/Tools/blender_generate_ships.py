"""Procedural ship generator for BOTF Remastered: original, faction-styled hulls for every roster/legacy asset key.

Run headless (Blender 4.2+ / 5.x):
  blender --background --factory-startup --python blender_generate_ships.py -- --out <dir> [--keys ship.fed.*,ship.kli.bop]
          [--preview]

For every key it writes <out>/<faction>/<slug>.fbx (nose +X, up +Z, 1 unit = 1 m, canon length) and, with --preview,
<out>/previews/<key>.png. <out>/generated-ships.json lists key, file, length, triangle count and sockets.
The design is original work built from a small hull kit (lathed hulls, convex-hull plates, swept arcs, greebles); it
does not copy any third-party mesh. Everything is deterministic: a key always produces the same ship.

Vertex colour channels (read by M_ShipHull, ue_import_generated_ships.py):
  R  window lights (the material's window pattern shows only where R is set)
  G  warm emitters: bussard collectors, impulse engines, torpedo launchers, disruptor tips
  B  cool emitters: warp grilles, deflector dishes, faction glow strips
  A  accent paint (faction trim colour)
Sockets: SOCKET_WPN_Beam_NN, SOCKET_WPN_Pulse_NN, SOCKET_WPN_Torp_NN at the real emitters (the battle scene fires from
them), SOCKET_ENG_NN at the engine exhausts.
"""
import fnmatch
import hashlib
import json
import math
import os
import sys

import bmesh
import bpy
import mathutils
from mathutils import Matrix, Vector

# ----------------------------------------------------------------------------------------------------------------
# Kit
# ----------------------------------------------------------------------------------------------------------------
HULL = (0.0, 0.0, 0.0, 0.0)
ACCENT = (0.0, 0.0, 0.0, 1.0)
WINDOWS = (1.0, 0.0, 0.0, 0.0)
WARM = (0.0, 1.0, 0.0, 0.0)
COOL = (0.0, 0.0, 1.0, 0.0)


class Ship:
    """One bmesh with a colour layer; primitives append to it."""

    def __init__(self, key, seed):
        self.key = key
        self.bm = bmesh.new()
        self.col = self.bm.loops.layers.color.new("Mask")
        self.sockets = []  # (name, Vector)
        self.rng = Rng(seed)

    # -- low level --------------------------------------------------------------------------------------------
    def _paint(self, faces, colour):
        for f in faces:
            for loop in f.loops:
                loop[self.col] = colour

    def _paint_fn(self, face_colours):
        for f, c in face_colours:
            for loop in f.loops:
                loop[self.col] = c

    def socket(self, kind, pos):
        n = sum(1 for s in self.sockets if s[0].startswith(kind)) + 1
        self.sockets.append(("%s_%02d" % (kind, n), Vector(pos)))

    # -- primitives ---------------------------------------------------------------------------------------------
    def lathe(self, profile, matrix=Matrix(), segments=32, squash=(1.0, 1.0), colour=HULL, colour_fn=None,
              smooth=True, arc=None):
        """Revolves profile [(x, r), ...] around local +X. r = 0 at either end closes with a pole.
        colour_fn(i_ring, n_rings, j_seg, n_seg) -> RGBA per face (overrides colour). arc=(a0, a1) keeps a sector."""
        bm = self.bm
        rings = []
        full = arc is None
        n_seg = segments
        for (x, r) in profile:
            ring = []
            if r <= 1e-6:
                ring = [bm.verts.new(matrix @ Vector((x, 0.0, 0.0)))]
            else:
                for j in range(n_seg + (0 if full else 1)):
                    if full:
                        a = 2 * math.pi * j / n_seg
                    else:
                        a = arc[0] + (arc[1] - arc[0]) * j / n_seg
                    ring.append(bm.verts.new(matrix @ Vector((x, r * math.cos(a) * squash[0], r * math.sin(a) * squash[1]))))
            rings.append(ring)
        faces = []
        n_r = len(rings)
        for i in range(n_r - 1):
            a, b = rings[i], rings[i + 1]
            cnt = n_seg if full else n_seg
            for j in range(cnt):
                jn = (j + 1) % n_seg if full else j + 1
                if len(a) == 1 and len(b) == 1:
                    continue
                if len(a) == 1:
                    f = bm.faces.new((a[0], b[jn], b[j]))
                elif len(b) == 1:
                    f = bm.faces.new((a[j], a[jn], b[0]))
                else:
                    f = bm.faces.new((a[j], a[jn], b[jn], b[j]))
                f.smooth = smooth
                faces.append((f, colour_fn(i, n_r, j, n_seg) if colour_fn else colour))
        if not full:  # close the sector's two open sides with flat caps
            for side in (0, -1):
                edge = [ring[side] if len(ring) > 1 else ring[0] for ring in rings]
                centre_line = [bm.verts.new(matrix @ Vector((x, 0.0, 0.0))) for (x, r) in profile]
                for i in range(n_r - 1):
                    quad = [centre_line[i], edge[i], edge[i + 1], centre_line[i + 1]]
                    if side == -1:
                        quad.reverse()
                    try:
                        f = bm.faces.new(quad)
                        f.smooth = False
                        faces.append((f, colour))
                    except ValueError:
                        pass
        self._paint_fn(faces)
        return [f for f, _ in faces]

    def plate(self, points, matrix=Matrix(), colour=HULL, smooth=False):
        """Convex hull of points: plates, wings, wedges, boxes."""
        vs = [self.bm.verts.new(matrix @ Vector(p)) for p in points]
        res = bmesh.ops.convex_hull(self.bm, input=vs)
        faces = [g for g in res["geom"] if isinstance(g, bmesh.types.BMFace)]
        for g in res.get("geom_interior", []) + res.get("geom_unused", []):
            if isinstance(g, bmesh.types.BMVert) and g.is_valid and not g.link_faces:
                self.bm.verts.remove(g)
        for f in faces:
            f.smooth = smooth
        self._paint(faces, colour)
        return faces

    def box(self, centre, size, matrix=Matrix(), colour=HULL, taper=(1.0, 1.0)):
        cx, cy, cz = centre
        sx, sy, sz = (s * 0.5 for s in size)
        tx, tz = taper  # front face scale (y, z)
        pts = [(cx - sx, cy - sy, cz - sz), (cx - sx, cy + sy, cz - sz), (cx - sx, cy - sy, cz + sz), (cx - sx, cy + sy, cz + sz),
               (cx + sx, cy - sy * tx, cz - sz * tz), (cx + sx, cy + sy * tx, cz - sz * tz),
               (cx + sx, cy - sy * tx, cz + sz * tz), (cx + sx, cy + sy * tx, cz + sz * tz)]
        return self.plate(pts, matrix, colour)

    def wing(self, root_front, root_back, tip_front, tip_back, t_root, t_tip, colour=HULL, matrix=Matrix()):
        """Aerofoil-ish slab between a root chord and a tip chord (points in the wing's plane), thickness along Z."""
        pts = []
        for p, t in ((root_front, t_root), (root_back, t_root), (tip_front, t_tip), (tip_back, t_tip)):
            pts.append((p[0], p[1], p[2] + t * 0.5))
            pts.append((p[0], p[1], p[2] - t * 0.5))
        return self.plate(pts, matrix, colour)

    def tube(self, p0, p1, r0, r1=None, segments=16, colour=HULL, caps=True, squash=(1.0, 1.0)):
        """Tapered cylinder from p0 to p1 (local frame aligned to the segment)."""
        p0, p1 = Vector(p0), Vector(p1)
        d = p1 - p0
        length = d.length
        if length < 1e-6:
            return []
        r1 = r0 if r1 is None else r1
        rot = Vector((1, 0, 0)).rotation_difference(d.normalized()).to_matrix().to_4x4()
        m = Matrix.Translation(p0) @ rot
        prof = ([(0.0, 0.0)] if caps else []) + [(0.0, r0), (length, r1)] + ([(length, 0.0)] if caps else [])
        return self.lathe(prof, m, segments, squash, colour, smooth=True)

    def ellipsoid(self, centre, radii, segments=24, rings=12, colour=HULL, colour_fn=None):
        prof = []
        for i in range(rings + 1):
            a = math.pi * i / rings
            prof.append((-math.cos(a) * radii[0], math.sin(a)))
        m = Matrix.Translation(Vector(centre))
        return self.lathe(prof, m, segments, (radii[1], radii[2]), colour, colour_fn)

    def arc_sweep(self, radius, a0, a1, section, matrix=Matrix(), steps=24, colour=HULL, taper=None):
        """Sweeps an elliptical section (w, h, n) along an arc of `radius` in the local XY plane."""
        w, h, n = section
        bm = self.bm
        rings = []
        for i in range(steps + 1):
            u = i / steps
            a = a0 + (a1 - a0) * u
            s = taper(u) if taper else 1.0
            centre = Vector((radius * math.cos(a), radius * math.sin(a), 0.0))
            outward = Vector((math.cos(a), math.sin(a), 0.0))
            ring = []
            for j in range(n):
                t = 2 * math.pi * j / n
                p = centre + outward * (w * s * math.cos(t)) + Vector((0, 0, h * s * math.sin(t)))
                ring.append(bm.verts.new(matrix @ p))
            rings.append(ring)
        faces = []
        for i in range(steps):
            for j in range(n):
                jn = (j + 1) % n
                f = bm.faces.new((rings[i][j], rings[i + 1][j], rings[i + 1][jn], rings[i][jn]))
                f.smooth = True
                faces.append(f)
        for ring, rev in ((rings[0], True), (rings[-1], False)):
            f = bm.faces.new(list(reversed(ring)) if rev else ring)
            f.smooth = False
            faces.append(f)
        self._paint(faces, colour)
        return faces

    def greebles(self, faces, count, size, up_only=False, colour=HULL):
        """Small blocks on a surface: bridges, sensor pods, plating."""
        cands = [f for f in faces if f.is_valid and f.calc_area() > (size * size * 0.5) and (not up_only or f.normal.z > 0.35)]
        if not cands:
            return
        for _ in range(count):
            f = cands[self.rng.int(len(cands))]
            c = f.calc_center_median()
            nrm = f.normal
            s = size * self.rng.range(0.4, 1.0)
            h = s * self.rng.range(0.2, 0.7)
            tangent = (f.verts[1].co - f.verts[0].co).normalized()
            bit = nrm.cross(tangent)
            m = Matrix((tangent, bit, nrm)).transposed().to_4x4()  # columns: local X, Y, Z
            m.translation = c
            self.box((0, 0, h * 0.4), (s * self.rng.range(0.6, 1.6), s * self.rng.range(0.4, 1.0), h), m, colour)

    # -- finish -------------------------------------------------------------------------------------------------
    def to_object(self, name, length):
        bm = self.bm
        bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-5)
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
        # Armour panelling: large flat faces (plates, wings, boxes; lathed skins keep their curvature) get an inset
        # panel, raised or recessed, so angular hulls read as plated rather than as bare facets.
        min_area = (length * 0.03) ** 2
        flat = [f for f in bm.faces if not f.smooth and f.calc_area() > min_area]
        for f in flat:
            if not f.is_valid or self.rng.next() > 0.8:
                continue
            size = math.sqrt(f.calc_area())
            depth = size * self.rng.range(0.006, 0.02) * (1 if self.rng.next() < 0.6 else -1)
            bmesh.ops.inset_individual(bm, faces=[f], thickness=size * self.rng.range(0.05, 0.12), depth=depth,
                                       use_even_offset=True)
            if self.rng.next() < 0.35 and f.is_valid and f.calc_area() > min_area * 4:  # a second, smaller panel
                bmesh.ops.inset_individual(bm, faces=[f], thickness=math.sqrt(f.calc_area()) * 0.1, depth=-depth * 0.6,
                                           use_even_offset=True)
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
        # Box-projected UVs at a constant texel density (panel texture tiles every `tile` metres).
        uv = bm.loops.layers.uv.new("UVMap")
        tile = max(4.0, length / 14.0)
        for f in bm.faces:
            n = f.normal
            ax = max(range(3), key=lambda i: abs(n[i]))
            u_i, v_i = [(1, 2), (0, 2), (0, 1)][ax]
            for loop in f.loops:
                loop[uv].uv = (loop.vert.co[u_i] / tile, loop.vert.co[v_i] / tile)
        mesh = bpy.data.meshes.new(name)
        bm.to_mesh(mesh)
        bm.free()
        ob = bpy.data.objects.new(name, mesh)
        bpy.context.scene.collection.objects.link(ob)
        return ob


class Rng:
    def __init__(self, seed):
        self.s = seed & 0xFFFFFFFF or 1

    def next(self):
        self.s = (self.s * 1664525 + 1013904223) & 0xFFFFFFFF
        return self.s / 4294967296.0

    def range(self, a, b):
        return a + (b - a) * self.next()

    def int(self, n):
        return int(self.next() * n) % max(1, n)

    def pick(self, seq):
        return seq[self.int(len(seq))]


def key_seed(key):
    return int(hashlib.sha1(key.encode()).hexdigest()[:8], 16)


# ----------------------------------------------------------------------------------------------------------------
# Federation
# ----------------------------------------------------------------------------------------------------------------
def fed_saucer(s, cx, cz, radius, thick, shape=1.0, windows=True, squash_y=1.0):
    """Lathed saucer around +Z at (cx, cz); a window band on the rim, bridge dome on top, impulse deck aft."""
    # Stepped profile (height, radius), bottom to top: ventral curve, rim band, dorsal slope, raised upper tier, bridge.
    t, R = thick, radius
    prof = [(-t * 0.70, 0.0), (-t * 0.68, R * 0.10), (-t * 0.62, R * 0.30), (-t * 0.48, R * 0.60), (-t * 0.30, R * 0.84),
            (-t * 0.14, R * 0.96), (-t * 0.04, R), (t * 0.04, R), (t * 0.16, R * 0.97), (t * 0.22, R * 0.935),
            (t * 0.28, R * 0.90), (t * 0.34, R * 0.86), (t * 0.46, R * 0.70), (t * 0.52, R * 0.62), (t * 0.62, R * 0.60),
            (t * 0.72, R * 0.45), (t * 0.80, R * 0.25), (t * 0.84, R * 0.12), (t * 0.95, R * 0.10), (t * 1.05, R * 0.06),
            (t * 1.08, 0.0)]
    m = Matrix.Translation((cx, 0, cz)) @ Matrix.Rotation(-math.pi / 2, 4, "Y")  # lathe axis -> +Z

    def colour(i, n, j, nseg):
        if windows and i in (4, 9, 13, 18):  # two rim decks (narrow bands), the upper-tier wall, the bridge
            return WINDOWS
        if i == 6:
            return ACCENT
        return HULL

    faces = s.lathe(prof, m, 64, (1.0 * shape, 1.0 * squash_y), colour_fn=colour)
    s.greebles([f for f in faces if f.normal.z > 0.5], 14, R * 0.05, up_only=True)
    # Dorsal and ventral phaser strips (four arcs), the ventral sensor dome.
    for z, rr in ((t * 0.37, R * 0.82), (-t * 0.40, R * 0.72)):
        for a0 in (0.15, math.pi + 0.15):
            s.arc_sweep(rr, a0, a0 + math.pi - 0.3, (R * 0.012, t * 0.035, 6),
                        Matrix.Translation((cx, 0, cz + z)) @ Matrix.Scale(squash_y, 4, Vector((1, 0, 0))) @ Matrix.Scale(shape, 4, Vector((0, 1, 0))),
                        steps=28, colour=ACCENT)
    s.ellipsoid((cx, 0, cz - t * 0.72), (R * 0.07, R * 0.07, t * 0.12), colour=COOL)
    # Phaser strips: the scene fires beams from these sockets.
    for k in range(4):
        a = math.pi * (0.25 + 0.5 * k)
        # The lathe squash stretches world X by squash_y (oval saucers are longer) and world Y by shape.
        s.socket("WPN_Beam", (cx + radius * 0.75 * math.cos(a) * squash_y, radius * 0.75 * math.sin(a) * shape, cz + thick * 0.4))
        s.socket("WPN_Beam", (cx + radius * 0.75 * math.cos(a) * squash_y, radius * 0.75 * math.sin(a) * shape, cz - thick * 0.4))
    # Impulse engines on the aft rim.
    s.box((cx - radius * 0.93, 0, cz + thick * 0.05), (radius * 0.08, radius * 0.45 * squash_y, thick * 0.3), colour=WARM)
    s.socket("ENG", (cx - radius * 0.98, 0, cz))
    return faces


def fed_nacelle(s, x0, x1, y, z, r, bussard=True, grille_side=0.0):
    length = x1 - x0
    # Flattened oval body: tapered aft cap, long straight run, a raised collar behind the bussard.
    prof = [(x0, 0.0), (x0 + length * 0.01, r * 0.45), (x0 + length * 0.04, r * 0.8), (x0 + length * 0.12, r),
            (x1 - length * 0.17, r), (x1 - length * 0.14, r * 1.08), (x1 - length * 0.1, r * 1.08),
            (x1 - length * 0.07, r * 0.95), (x1 - length * 0.02, r * 0.75), (x1, r * 0.35), (x1, 0.0)]

    def colour(i, n, j, nseg):
        return ACCENT if i == 5 else HULL

    s.lathe(prof, Matrix.Translation((0, y, z)), 24, (1.0, 0.8), colour_fn=colour)
    if bussard:
        s.ellipsoid((x1 - length * 0.035, y, z), (length * 0.07, r * 0.86, r * 0.72), colour=WARM)
    # Warp grilles: a long glowing strip on the inboard side (both sides when centred), and a dorsal spine.
    side = -1 if y > 0 else 1
    for gs in ((side,) if grille_side == 0.0 else (grille_side,)):
        s.box((x0 + length * 0.5, y + gs * r * 0.93, z), (length * 0.6, r * 0.12, r * 0.6), colour=COOL)
    s.box((x0 + length * 0.48, y, z + r * 0.78), (length * 0.66, r * 0.35, r * 0.14), colour=COOL)
    s.box((x0 + length * 0.02, y, z), (length * 0.03, r * 0.9, r * 0.6), colour=WARM)  # aft exhaust
    s.socket("ENG", (x0, y, z))


def fed_engineering(s, x0, x1, z, r, deflector=True, squash=(1.0, 1.0)):
    length = x1 - x0
    prof = [(x0, 0.0), (x0 + length * 0.02, r * 0.5), (x0 + length * 0.15, r * 0.85), (x0 + length * 0.45, r),
            (x1 - length * 0.18, r * 0.95), (x1 - length * 0.06, r * 0.8), (x1, r * 0.45), (x1, 0.0)]

    def colour(i, n, j, nseg):
        return WINDOWS if i == 3 and (j % 3 == 0) else HULL

    faces = s.lathe(prof, Matrix.Translation((0, 0, z)), 32, squash, colour_fn=colour)
    s.greebles([f for f in faces if abs(f.normal.y) > 0.6], 10, r * 0.14)
    if deflector:
        # Glowing dish inside a raised accent ring, facing forward.
        dz = z - r * 0.05 * squash[1]
        s.ellipsoid((x1 - length * 0.015, 0, dz), (length * 0.03, r * 0.5 * squash[0], r * 0.5 * squash[1]), colour=COOL)
        ring = Matrix.Translation((x1 - length * 0.02, 0, dz)) @ Matrix.Rotation(math.pi / 2, 4, "Y") @ \
            Matrix.Scale(squash[1], 4, Vector((1, 0, 0))) @ Matrix.Scale(squash[0], 4, Vector((0, 1, 0)))
        s.arc_sweep(r * 0.58, 0, 2 * math.pi, (r * 0.07, r * 0.07, 8), ring, steps=32, colour=ACCENT)
    # Shuttle bay doors at the stern.
    s.box((x0 + length * 0.03, 0, z + r * 0.2 * squash[1]), (length * 0.04, r * 0.7 * squash[0], r * 0.5 * squash[1]), colour=WINDOWS)
    s.socket("WPN_Torp", (x1 - length * 0.08, 0, z + r * 0.55 * squash[1]))
    s.socket("WPN_Torp", (x0 + length * 0.05, 0, z + r * 0.4 * squash[1]))
    s.socket("WPN_Beam", (x0 + length * 0.5, 0, z - r * squash[1]))
    return faces


def fed_pylon(s, a, b, chord, thick):
    a, b = Vector(a), Vector(b)
    s.wing((a.x + chord * 0.5, a.y, a.z), (a.x - chord * 0.5, a.y, a.z), (b.x + chord * 0.35, b.y, b.z),
           (b.x - chord * 0.45, b.y, b.z), thick, thick * 0.8)


def build_fed(s, p):
    L = p["length"]
    arch = p["arch"]
    r = s.rng
    if arch == "explorer":  # Galaxy / Sovereign / Ambassador / Luna / Olympic
        sr = L * p.get("saucer", 0.26)
        st = sr * 0.26
        sx = L * 0.5 - sr
        fed_saucer(s, sx, 0, sr, st, squash_y=p.get("oval", 1.0))
        ex0, ex1 = -L * 0.5 + L * 0.12, sx - sr * 0.15
        er = sr * 0.32
        ez = -st * 1.45
        fed_engineering(s, ex0, ex1, ez, er, squash=(1.0, 0.8))
        # Swept neck: wide where it meets the saucer's belly, narrowing onto the engineering hull's back.
        s.plate([(sx - sr * 0.15, er * 0.45, -st * 0.4), (sx - sr * 0.15, -er * 0.45, -st * 0.4),
                 (sx - sr * 0.85, er * 0.55, -st * 0.4), (sx - sr * 0.85, -er * 0.55, -st * 0.4),
                 (sx - sr * 0.55, er * 0.35, ez + er * 0.6), (sx - sr * 0.55, -er * 0.35, ez + er * 0.6),
                 (sx - sr * 1.25, er * 0.45, ez + er * 0.7), (sx - sr * 1.25, -er * 0.45, ez + er * 0.7)])
        s.box((sx - sr * 0.5, 0, (-st * 0.4 + ez) * 0.5), (sr * 0.5, er * 0.72, st * 0.5), colour=WINDOWS)
        ny = sr * p.get("span", 0.62)
        nz = -st * 0.4 + L * p.get("nac_z", 0.0)
        nx0, nx1 = -L * 0.5, -L * 0.5 + L * p.get("nac_len", 0.52)
        for side in (-1, 1):
            fed_nacelle(s, nx0, nx1, side * ny, nz, L * 0.028)
            fed_pylon(s, (ex0 + L * 0.2, side * er * 0.7, ez), (nx0 + L * 0.22, side * ny, nz), L * 0.12, L * 0.012)
    elif arch == "cruiser":  # Constitution / Excelsior / refit
        sr = L * p.get("saucer", 0.22)
        st = sr * 0.25
        sx = L * 0.5 - sr
        fed_saucer(s, sx, L * 0.02, sr, st)
        ex0, ex1 = -L * 0.35, sx - sr * 0.4
        er = L * 0.055
        fed_engineering(s, ex0, ex1, -L * 0.08, er)
        s.wing((sx - sr * 0.45, 0, L * 0.0), (sx - sr * 0.8, 0, L * 0.0), (ex1 - L * 0.02, 0, -L * 0.08 + er), (ex1 - L * 0.12, 0, -L * 0.08 + er), L * 0.02, L * 0.03)
        ny, nz = L * 0.15, L * 0.07
        for side in (-1, 1):
            fed_nacelle(s, -L * 0.5, -L * 0.5 + L * p.get("nac_len", 0.5), side * ny, nz, L * 0.028)
            fed_pylon(s, (ex0 + L * 0.12, side * er * 0.6, -L * 0.07), (-L * 0.5 + L * 0.18, side * ny, nz), L * 0.08, L * 0.012)
    elif arch == "frigate":  # Miranda / Soyuz / Constellation / Centaur / Freedom
        sr = L * p.get("saucer", 0.36)
        st = sr * 0.24
        sx = L * 0.5 - sr
        fed_saucer(s, sx, 0, sr, st)
        s.ellipsoid((sx - sr * 0.95, 0, -st * 0.1), (sr * 0.45, sr * 0.3, st * 0.9))
        n = p.get("nacelles", 2)
        ny = sr * 0.75
        levels = [st * 0.9] if n == 2 else [st * 1.1, -st * 1.1]
        for z in levels:
            for side in (-1, 1):
                fed_nacelle(s, -L * 0.5, -L * 0.5 + L * 0.46, side * ny, z * (1 if p.get("nac_up", False) or n == 4 else -1), L * 0.032)
        if p.get("rollbar", False):
            s.wing((sx - sr * 0.9, -ny, st * 1.5), (sx - sr * 1.05, -ny, st * 1.5), (sx - sr * 0.9, ny, st * 1.5), (sx - sr * 1.05, ny, st * 1.5), st * 0.4, st * 0.4)
            s.box((sx - sr * 0.97, 0, st * 1.9), (sr * 0.2, sr * 0.35, st * 0.8), colour=WARM)
            s.socket("WPN_Torp", (sx - sr * 0.9, 0, st * 2.0))
            for side in (-1, 1):
                s.box((sx - sr * 0.97, side * ny, st * 1.0), (sr * 0.12, sr * 0.08, st * 1.1))
        s.socket("WPN_Torp", (sx + sr * 0.2, 0, -st * 0.5))
    elif arch == "escort":  # Defiant / Saber / Yeager / Norway / Steamrunner / Akira / Prometheus
        w = L * p.get("width", 0.72)
        h = L * p.get("height", 0.2)
        nose = L * 0.5
        tail = -L * 0.5
        if p.get("pod", False):  # Akira: a broad saucer, twin catamaran arms, a torpedo pod bridging them
            sr = L * 0.33
            st = sr * 0.26
            sx = nose - sr
            fed_saucer(s, sx, 0, sr, st, shape=1.12)
            az = -st * 0.1
            for side in (-1, 1):
                s.plate([(sx - sr * 0.3, side * sr * 0.55, az + st * 0.3), (sx - sr * 0.3, side * sr * 0.55, az - st * 0.3),
                         (sx - sr * 0.3, side * sr * 0.9, az + st * 0.3), (sx - sr * 0.3, side * sr * 0.9, az - st * 0.3),
                         (tail + L * 0.05, side * w * 0.42, az + st * 0.35), (tail + L * 0.05, side * w * 0.42, az - st * 0.35),
                         (tail + L * 0.05, side * w * 0.58, az + st * 0.35), (tail + L * 0.05, side * w * 0.58, az - st * 0.35)])
                fed_nacelle(s, tail - L * 0.02, tail + L * 0.4, side * w * 0.5, az - st * 0.9, L * 0.028)
                s.box((tail + L * 0.2, side * w * 0.5, az + st * 1.0), (L * 0.12, w * 0.06, st * 1.4))  # pod strut
            pod = [(tail + L * 0.1, 0.0), (tail + L * 0.11, 0.6), (tail + L * 0.16, 1.0), (tail + L * 0.36, 1.0), (tail + L * 0.42, 0.55), (tail + L * 0.43, 0.0)]
            s.lathe([(x, r * st * 0.8) for x, r in pod], Matrix.Translation((0, 0, az + st * 1.9)), 24, (w * 0.62 / (st * 0.8), 1.0),
                    colour_fn=lambda i, n, j, ns: WINDOWS if i == 2 and j % 4 == 0 else HULL)
            for k in range(5):
                s.socket("WPN_Torp", (tail + L * 0.42, (k - 2) * w * 0.1, az + st * 1.9))
            s.socket("WPN_Beam", (sx, 0, st * 0.5))
        else:
            # Smooth arrowhead: a lathed body flattened into a wedge, a raised command deck with a window band, and
            # nacelles faired into the flanks.
            # Per-class variation (seeded by key): nose length, deck placement, nacelles above/below, tail fins.
            nose_k = r.range(0.28, 0.46)
            deck_x = r.range(-0.12, 0.1) * L
            nac_z = r.pick((-0.05, -0.05, 0.35, -0.4)) * h
            prof = [(tail, 0.0), (tail, 0.28), (tail + L * 0.04, 0.42), (tail + L * 0.3, 0.5), (nose - L * nose_k, 0.48),
                    (nose - L * nose_k * 0.5, 0.32), (nose - L * 0.05, 0.14), (nose, 0.0)]
            s.lathe([(x, rr * w) for x, rr in prof], Matrix(), 40, (1.0, h / w),
                    colour_fn=lambda i, n, j, ns: ACCENT if i == 4 and (j % (ns // 2)) < 2 else HULL)

            def deck_colour(i, n, j, ns):  # a window band running round the deck's equator
                return WINDOWS if 2 <= i <= n - 4 and j in (0, ns // 2 - 1, ns // 2, ns - 1) else HULL

            s.ellipsoid((deck_x, 0, h * 0.42), (L * r.range(0.15, 0.24), w * 0.16, h * 0.32), colour_fn=deck_colour)
            for side in (-1, 1):
                fed_nacelle(s, tail - L * 0.02, tail + L * 0.55, side * w * 0.46, nac_z, h * 0.3)
                if abs(nac_z) > h * 0.2:  # raised or dropped nacelles need a pylon
                    fed_pylon(s, (tail + L * 0.25, side * w * 0.3, 0), (tail + L * 0.25, side * w * 0.46, nac_z), L * 0.14, h * 0.12)
            for k in range(r.int(3)):  # dorsal fins
                fx = tail + L * (0.08 + 0.1 * k)
                ft = w * 0.012
                s.plate([(fx + L * 0.08, sy * ft, h * 0.3) for sy in (-1, 1)] + [(fx, sy * ft, h * 0.3) for sy in (-1, 1)] +
                        [(fx + L * 0.01, sy * ft * 0.6, h * 0.8) for sy in (-1, 1)] + [(fx - L * 0.03, sy * ft * 0.6, h * 0.8) for sy in (-1, 1)])
                s.socket("WPN_Pulse", (nose - L * 0.12, side * w * 0.15, h * 0.1))
            s.ellipsoid((nose - L * 0.06, 0, -h * 0.12), (L * 0.05, w * 0.1, h * 0.14), colour=COOL)  # navigational deflector
            s.socket("WPN_Torp", (nose - L * 0.05, 0, -h * 0.1))
            s.socket("WPN_Beam", (0, 0, h * 0.5))
    elif arch == "science":  # Nova / Oberth / Intrepid / Nebula / Olympic
        sr = L * p.get("saucer", 0.3)
        st = sr * 0.25
        sx = L * 0.5 - sr
        fed_saucer(s, sx, 0, sr, st, squash_y=p.get("oval", 1.0))
        if p.get("oberth", False):
            ny = sr * 0.35
            for side in (-1, 1):
                fed_nacelle(s, -L * 0.5, sx - sr * 0.2, side * ny, st * 0.8, L * 0.035, grille_side=side)
            s.ellipsoid((sx - sr * 0.4, 0, -sr * 0.55), (sr * 1.1, sr * 0.22, sr * 0.14))
            s.box((sx - sr * 0.4, 0, -sr * 0.25), (sr * 0.2, sr * 0.08, sr * 0.5))
        else:
            fed_engineering(s, -L * 0.42, sx - sr * 0.4, -st * 1.1, sr * 0.3, squash=(1.1, 0.7))
            ny = sr * 0.7
            for side in (-1, 1):
                fed_nacelle(s, -L * 0.5, -L * 0.5 + L * 0.42, side * ny, -st * (0.2 if not p.get("nac_up") else -1.2), L * 0.03)
                fed_pylon(s, (-L * 0.25, side * sr * 0.25, -st * 1.0), (-L * 0.3, side * ny, -st * 0.2), L * 0.1, L * 0.012)
            if p.get("pod", False):  # Nebula mission pod
                s.box((-L * 0.18, 0, st * 1.9), (L * 0.22, sr * 0.9, st * 1.1), colour=HULL)
                s.box((-L * 0.18, 0, st * 1.1), (L * 0.08, sr * 0.1, st * 1.4))
                s.ellipsoid((-L * 0.18, 0, st * 2.6), (L * 0.05, L * 0.05, st * 0.35), colour=COOL)
    elif arch == "runabout":  # Danube / Y-class / Sydney / Raven / small transports
        w, h = L * 0.3, L * 0.14
        s.plate([(L * 0.5, 0, 0), (L * 0.4, w * 0.45, h * 0.4), (L * 0.4, -w * 0.45, h * 0.4), (L * 0.4, w * 0.5, -h * 0.5), (L * 0.4, -w * 0.5, -h * 0.5),
                 (-L * 0.45, w * 0.5, h * 0.5), (-L * 0.45, -w * 0.5, h * 0.5), (-L * 0.45, w * 0.5, -h * 0.5), (-L * 0.45, -w * 0.5, -h * 0.5)])
        s.box((L * 0.38, 0, h * 0.2), (L * 0.06, w * 0.7, h * 0.3), colour=WINDOWS)
        for side in (-1, 1):
            fed_nacelle(s, -L * 0.5, L * 0.2, side * w * 0.75, -h * 0.1, h * 0.28)
        s.socket("WPN_Beam", (L * 0.3, 0, -h * 0.5))
        s.socket("WPN_Torp", (L * 0.45, 0, 0))
    elif arch == "station":
        build_station(s, p, dome=True)
    return s


# ----------------------------------------------------------------------------------------------------------------
# Klingon
# ----------------------------------------------------------------------------------------------------------------
def kli_head(s, x, z, size, windows=True):
    s.plate([(x + size * 1.1, 0, z), (x + size * 0.5, size * 0.55, z + size * 0.25), (x + size * 0.5, -size * 0.55, z + size * 0.25),
             (x + size * 0.5, size * 0.6, z - size * 0.3), (x + size * 0.5, -size * 0.6, z - size * 0.3),
             (x - size * 0.4, size * 0.45, z + size * 0.35), (x - size * 0.4, -size * 0.45, z + size * 0.35),
             (x - size * 0.4, size * 0.45, z - size * 0.35), (x - size * 0.4, -size * 0.45, z - size * 0.35)])
    s.box((x + size * 0.3, 0, z + size * 0.38), (size * 0.5, size * 0.35, size * 0.12), colour=WINDOWS if windows else HULL)
    s.box((x + size * 1.05, 0, z - size * 0.08), (size * 0.15, size * 0.18, size * 0.14), colour=WARM)
    s.socket("WPN_Torp", (x + size * 1.12, 0, z - size * 0.08))


def build_kli(s, p):
    L = p["length"]
    arch = p["arch"]
    if arch == "bop":  # Bird-of-prey: head on a neck, gull wings with tip disruptors
        neck_x0, neck_x1 = -L * 0.05, L * 0.3
        kli_head(s, L * 0.32, 0, L * 0.12)
        s.tube((neck_x0, 0, L * 0.02), (neck_x1, 0, 0), L * 0.05, L * 0.035, segments=12)
        s.plate([(L * 0.05, L * 0.1, L * 0.06), (L * 0.05, -L * 0.1, L * 0.06), (L * 0.05, L * 0.11, -L * 0.04), (L * 0.05, -L * 0.11, -L * 0.04),
                 (-L * 0.45, L * 0.16, L * 0.08), (-L * 0.45, -L * 0.16, L * 0.08), (-L * 0.45, L * 0.14, -L * 0.06), (-L * 0.45, -L * 0.14, -L * 0.06),
                 (-L * 0.5, 0, 0)])
        s.box((-L * 0.47, 0, 0), (L * 0.04, L * 0.2, L * 0.07), colour=WARM)
        s.socket("ENG", (-L * 0.5, 0, 0))
        dihedral = p.get("dihedral", -0.35)
        span = L * p.get("span", 0.62)
        for side in (-1, 1):
            tip_y, tip_z = side * span, span * dihedral
            s.wing((-L * 0.02, side * L * 0.1, 0), (-L * 0.38, side * L * 0.14, 0), (-L * 0.15, tip_y, tip_z), (-L * 0.32, tip_y, tip_z), L * 0.05, L * 0.02, colour=ACCENT)
            s.wing((-L * 0.1, side * L * 0.2, L * 0.01), (-L * 0.3, side * L * 0.22, L * 0.01), (-L * 0.2, side * span * 0.7, tip_z * 0.7 + L * 0.01),
                   (-L * 0.28, side * span * 0.7, tip_z * 0.7 + L * 0.01), L * 0.02, L * 0.01)  # rib
            s.tube((-L * 0.25, tip_y, tip_z), (L * 0.05, tip_y, tip_z), L * 0.022, L * 0.016, segments=10)
            s.ellipsoid((L * 0.06, tip_y, tip_z), (L * 0.02, L * 0.016, L * 0.016), colour=WARM)
            s.socket("WPN_Pulse", (L * 0.08, tip_y, tip_z))
    else:  # battlecruiser: D7 / K't'inga / Vor'cha / Negh'Var / D5 / Qang
        head = L * p.get("head", 0.1)
        neck = p.get("neck", 0.4)
        kli_head(s, L * 0.5 - head * 1.1, L * 0.02, head)
        hx = L * 0.5 - head * 1.4
        nx = hx - L * neck
        s.plate([(hx, L * 0.03, L * 0.04), (hx, -L * 0.03, L * 0.04), (hx, L * 0.035, -L * 0.02), (hx, -L * 0.035, -L * 0.02),
                 (nx, L * 0.06, L * 0.05), (nx, -L * 0.06, L * 0.05), (nx, L * 0.07, -L * 0.04), (nx, -L * 0.07, -L * 0.04)])
        # Aft hull: a flattened wedge flaring into the wing roots.
        ax0, ax1 = nx + L * 0.05, -L * 0.5
        wide = L * p.get("aft_width", 0.3)
        faces = s.plate([(ax0, L * 0.06, L * 0.06), (ax0, -L * 0.06, L * 0.06), (ax0, L * 0.07, -L * 0.07), (ax0, -L * 0.07, -L * 0.07),
                         (ax1 + L * 0.12, wide * 0.5, L * 0.05), (ax1 + L * 0.12, -wide * 0.5, L * 0.05), (ax1 + L * 0.12, wide * 0.45, -L * 0.07),
                         (ax1 + L * 0.12, -wide * 0.45, -L * 0.07), (ax1, wide * 0.3, L * 0.03), (ax1, -wide * 0.3, L * 0.03), (ax1, wide * 0.25, -L * 0.05),
                         (ax1, -wide * 0.25, -L * 0.05), (ax0 - L * 0.05, 0, L * 0.12)])
        s.greebles(faces, 12, L * 0.03, up_only=True)
        s.box((ax1 + L * 0.02, 0, 0), (L * 0.05, wide * 0.3, L * 0.07), colour=WARM)
        s.socket("ENG", (ax1, 0, 0))
        span = wide * p.get("span", 0.75)
        drop = p.get("wing_drop", 0.25)
        for side in (-1, 1):
            s.wing((ax1 + L * 0.3, side * wide * 0.35, 0), (ax1 + L * 0.08, side * wide * 0.4, 0),
                   (ax1 + L * 0.18, side * (wide * 0.35 + span), -span * drop), (ax1 + L * 0.02, side * (wide * 0.35 + span), -span * drop), L * 0.04, L * 0.02, colour=ACCENT)
            ny = side * (wide * 0.35 + span)
            nz = -span * drop
            s.tube((ax1 - L * 0.02, ny, nz + L * 0.01), (ax1 + L * 0.28, ny, nz + L * 0.01), L * 0.03, L * 0.024, segments=12)
            s.box((ax1 + L * 0.16, ny - side * L * 0.028, nz + L * 0.01), (L * 0.2, L * 0.006, L * 0.035), colour=WARM)
            s.socket("WPN_Pulse", (ax1 + L * 0.3, ny, nz))
            s.socket("WPN_Beam", (hx - L * 0.02, side * L * 0.035, 0))
        s.socket("WPN_Torp", (ax1 + L * 0.02, 0, L * 0.05))
    return s


# ----------------------------------------------------------------------------------------------------------------
# Romulan
# ----------------------------------------------------------------------------------------------------------------
def build_rom(s, p):
    L = p["length"]
    arch = p["arch"]
    if arch == "warbird":  # D'deridex: two swept hulls meeting fore and aft around an open centre
        span = L * p.get("span", 0.55)
        gap = L * 0.13
        for sgn in (1, -1):
            z = sgn * gap
            s.plate([(L * 0.2, 0, z * 0.2), (L * 0.05, span * 0.25, z * 0.7), (L * 0.05, -span * 0.25, z * 0.7), (-L * 0.25, span * 0.5, z),
                     (-L * 0.25, -span * 0.5, z), (-L * 0.5, span * 0.4, z * 0.6), (-L * 0.5, -span * 0.4, z * 0.6),
                     (-L * 0.1, 0, z + sgn * L * 0.05), (-L * 0.4, 0, z * 0.8 + sgn * L * 0.03)], colour=HULL if sgn > 0 else ACCENT)
        s.tube((L * 0.18, 0, 0), (L * 0.38, 0, 0), L * 0.04, L * 0.03, segments=12)
        s.plate([(L * 0.5, 0, 0), (L * 0.36, L * 0.07, L * 0.03), (L * 0.36, -L * 0.07, L * 0.03), (L * 0.36, L * 0.07, -L * 0.03),
                 (L * 0.36, -L * 0.07, -L * 0.03), (L * 0.42, 0, L * 0.05)])
        s.box((L * 0.43, 0, L * 0.035), (L * 0.05, L * 0.05, L * 0.015), colour=WINDOWS)
        s.socket("WPN_Torp", (L * 0.5, 0, 0))
        for side in (-1, 1):
            s.tube((-L * 0.5, side * span * 0.42, 0), (-L * 0.2, side * span * 0.45, 0), L * 0.03, L * 0.02, segments=10)
            s.box((-L * 0.35, side * span * 0.43, 0), (L * 0.25, L * 0.02, L * 0.02), colour=COOL)
            s.socket("WPN_Beam", (-L * 0.1, side * span * 0.3, gap))
            s.socket("WPN_Beam", (-L * 0.1, side * span * 0.3, -gap))
        s.ellipsoid((-L * 0.1, 0, 0), (L * 0.08, L * 0.08, gap * 0.6), colour=COOL)  # singularity core glow
        s.socket("ENG", (-L * 0.5, 0, 0))
    else:  # bird: swept wings, head forward (bop / valdore / mogai / norexan / scout / science / drone)
        span = L * p.get("span", 0.9)
        sweep = p.get("sweep", 0.35)
        body_w = L * 0.12
        s.plate([(L * 0.5, 0, 0), (L * 0.3, body_w, L * 0.04), (L * 0.3, -body_w, L * 0.04), (L * 0.3, body_w * 0.8, -L * 0.04),
                 (L * 0.3, -body_w * 0.8, -L * 0.04), (-L * 0.45, body_w * 0.7, L * 0.05), (-L * 0.45, -body_w * 0.7, L * 0.05),
                 (-L * 0.45, body_w * 0.6, -L * 0.04), (-L * 0.45, -body_w * 0.6, -L * 0.04), (L * 0.1, 0, L * 0.08)])
        s.box((L * 0.28, 0, L * 0.05), (L * 0.08, body_w * 0.8, L * 0.02), colour=WINDOWS)
        for side in (-1, 1):
            tip = (-L * sweep, side * span * 0.5, L * p.get("lift", 0.05))
            s.wing((L * 0.2, side * body_w * 0.9, 0), (-L * 0.4, side * body_w * 0.7, 0), (tip[0] + L * 0.12, tip[1], tip[2]), (tip[0] - L * 0.12, tip[1], tip[2]),
                   L * 0.05, L * 0.012, colour=ACCENT)
            s.tube((tip[0] - L * 0.15, tip[1], tip[2]), (tip[0] + L * 0.2, tip[1], tip[2]), L * 0.025, L * 0.018, segments=10)
            s.box((tip[0] + L * 0.02, tip[1] - side * L * 0.024, tip[2]), (L * 0.25, L * 0.005, L * 0.02), colour=COOL)
            s.socket("WPN_Beam", (tip[0] + L * 0.2, tip[1], tip[2]))
        s.box((-L * 0.45, 0, 0), (L * 0.04, body_w, L * 0.05), colour=COOL)
        s.socket("WPN_Torp", (L * 0.5, 0, 0))
        s.socket("ENG", (-L * 0.46, 0, 0))
    return s


# ----------------------------------------------------------------------------------------------------------------
# Cardassian
# ----------------------------------------------------------------------------------------------------------------
def build_car(s, p):
    L = p["length"]
    arch = p["arch"]
    if arch == "galor":  # long forward spear, flared aft wings, raised "head" over the aft hull
        w = L * p.get("width", 0.42)
        faces = s.plate([(L * 0.5, 0, 0), (L * 0.1, w * 0.12, L * 0.035), (L * 0.1, -w * 0.12, L * 0.035), (L * 0.1, w * 0.1, -L * 0.03),
                         (L * 0.1, -w * 0.1, -L * 0.03), (-L * 0.2, w * 0.5, L * 0.02), (-L * 0.2, -w * 0.5, L * 0.02), (-L * 0.2, w * 0.45, -L * 0.03),
                         (-L * 0.2, -w * 0.45, -L * 0.03), (-L * 0.5, w * 0.2, L * 0.02), (-L * 0.5, -w * 0.2, L * 0.02),
                         (-L * 0.5, w * 0.2, -L * 0.03), (-L * 0.5, -w * 0.2, -L * 0.03)])
        s.greebles(faces, 14, L * 0.02, up_only=True)
        s.plate([(L * 0.05, 0, L * 0.04), (-L * 0.1, w * 0.18, L * 0.1), (-L * 0.1, -w * 0.18, L * 0.1), (-L * 0.45, w * 0.15, L * 0.06),
                 (-L * 0.45, -w * 0.15, L * 0.06), (-L * 0.3, 0, L * 0.14), (-L * 0.45, 0, L * 0.02), (L * 0.05, 0, L * 0.02)], colour=ACCENT)
        s.box((-L * 0.12, 0, L * 0.11), (L * 0.08, w * 0.12, L * 0.02), colour=WINDOWS)
        s.box((L * 0.47, 0, 0), (L * 0.05, L * 0.02, L * 0.02), colour=WARM)  # spinal disruptor emitter
        s.socket("WPN_Beam", (L * 0.5, 0, 0))
        s.socket("WPN_Torp", (L * 0.1, 0, -L * 0.03))
        for side in (-1, 1):
            s.box((-L * 0.35, side * w * 0.33, -L * 0.035), (L * 0.2, w * 0.1, L * 0.02), colour=WARM)
            s.socket("WPN_Beam", (-L * 0.2, side * w * 0.48, 0))
        s.box((-L * 0.49, 0, 0), (L * 0.03, w * 0.35, L * 0.04), colour=WARM)
        s.socket("ENG", (-L * 0.5, 0, 0))
        if p.get("keldon", False):
            s.box((-L * 0.52, 0, L * 0.05), (L * 0.12, w * 0.5, L * 0.03), colour=ACCENT)
    else:  # hideki / scout / small craft: a flattened scarab
        w = L * 0.55
        s.ellipsoid((0, 0, 0), (L * 0.5, w * 0.5, L * 0.08), segments=20, rings=10)
        s.plate([(L * 0.2, 0, L * 0.07), (-L * 0.3, w * 0.25, L * 0.06), (-L * 0.3, -w * 0.25, L * 0.06), (-L * 0.1, 0, L * 0.13)], colour=ACCENT)
        s.box((L * 0.3, 0, L * 0.02), (L * 0.1, w * 0.2, L * 0.03), colour=WINDOWS)
        s.box((-L * 0.48, 0, 0), (L * 0.04, w * 0.4, L * 0.05), colour=WARM)
        s.socket("WPN_Pulse", (L * 0.48, 0, 0))
        s.socket("ENG", (-L * 0.5, 0, 0))
    return s


# ----------------------------------------------------------------------------------------------------------------
# Ferengi
# ----------------------------------------------------------------------------------------------------------------
def build_fer(s, p):
    L = p["length"]
    arch = p["arch"]
    if arch == "marauder":  # D'Kora: a horseshoe crescent, prongs forward, command pod at the apex
        R = L * 0.45
        rot = Matrix.Rotation(math.pi, 4, "Z")  # arc opens forward (+X)
        s.arc_sweep(R, -2.3, 2.3, (L * 0.15, L * 0.07, 16), Matrix.Translation((L * 0.05, 0, 0)) @ rot, steps=40,
                    taper=lambda u: 0.55 + 0.45 * math.sin(math.pi * u))
        s.ellipsoid((-L * 0.36, 0, L * 0.04), (L * 0.14, L * 0.12, L * 0.07), colour=HULL)
        s.box((-L * 0.3, 0, L * 0.09), (L * 0.1, L * 0.1, L * 0.03), colour=WINDOWS)
        s.plate([(-L * 0.45, L * 0.2, 0), (-L * 0.45, -L * 0.2, 0), (-L * 0.5, 0, L * 0.03), (-L * 0.5, 0, -L * 0.03), (-L * 0.25, 0, L * 0.02)], colour=ACCENT)
        for side in (-1, 1):
            a = side * 2.25
            tip = (L * 0.05 - R * math.cos(a), -R * math.sin(a), 0)
            s.ellipsoid(tip, (L * 0.035, L * 0.03, L * 0.03), colour=WARM)
            s.socket("WPN_Beam", tip)
            s.box((-L * 0.25, side * L * 0.3, -L * 0.03), (L * 0.18, L * 0.02, L * 0.02), colour=COOL)
        s.box((-L * 0.49, 0, 0), (L * 0.03, L * 0.2, L * 0.04), colour=WARM)
        s.socket("WPN_Torp", (-L * 0.2, 0, -L * 0.05))
        s.socket("ENG", (-L * 0.5, 0, 0))
    else:  # pod ship / shuttle / scout
        s.ellipsoid((0, 0, 0), (L * 0.5, L * 0.22, L * 0.12))
        s.box((L * 0.35, 0, L * 0.06), (L * 0.1, L * 0.12, L * 0.03), colour=WINDOWS)
        for side in (-1, 1):
            s.wing((L * 0.1, side * L * 0.15, 0), (-L * 0.3, side * L * 0.18, 0), (-L * 0.1, side * L * 0.42, -L * 0.05), (-L * 0.35, side * L * 0.42, -L * 0.05), L * 0.03, L * 0.015, colour=ACCENT)
        s.box((-L * 0.48, 0, 0), (L * 0.04, L * 0.2, L * 0.06), colour=WARM)
        s.socket("WPN_Beam", (L * 0.48, 0, 0))
        s.socket("ENG", (-L * 0.5, 0, 0))
    return s


# ----------------------------------------------------------------------------------------------------------------
# Dominion, Borg, stations, generic aliens
# ----------------------------------------------------------------------------------------------------------------
def build_dom(s, p):
    L = p["length"]
    if p["arch"] == "attack":  # beetle: rounded body, forward mandibles, glowing dorsal emitter
        s.ellipsoid((0, 0, 0), (L * 0.42, L * 0.3, L * 0.12), segments=24, rings=12)
        for side in (-1, 1):
            s.plate([(L * 0.5, side * L * 0.12, 0), (L * 0.2, side * L * 0.28, L * 0.03), (L * 0.2, side * L * 0.2, -L * 0.04), (-L * 0.1, side * L * 0.3, 0)], colour=ACCENT)
            s.socket("WPN_Pulse", (L * 0.5, side * L * 0.12, 0))
            s.tube((-L * 0.5, side * L * 0.2, -L * 0.02), (-L * 0.1, side * L * 0.25, -L * 0.02), L * 0.04, L * 0.035, segments=10)
            s.box((-L * 0.3, side * L * 0.25, -L * 0.06), (L * 0.3, L * 0.04, L * 0.01), colour=COOL)
        s.box((L * 0.05, 0, L * 0.12), (L * 0.35, L * 0.05, L * 0.02), colour=COOL)
        s.socket("WPN_Beam", (L * 0.2, 0, L * 0.13))
        s.socket("WPN_Torp", (L * 0.42, 0, -L * 0.03))
    else:  # battlecruiser / battleship: long prowed hull with a forked aft
        faces = s.plate([(L * 0.5, 0, 0), (L * 0.2, L * 0.12, L * 0.06), (L * 0.2, -L * 0.12, L * 0.06), (L * 0.2, L * 0.1, -L * 0.06), (L * 0.2, -L * 0.1, -L * 0.06),
                         (-L * 0.3, L * 0.2, L * 0.07), (-L * 0.3, -L * 0.2, L * 0.07), (-L * 0.3, L * 0.18, -L * 0.07), (-L * 0.3, -L * 0.18, -L * 0.07)])
        s.greebles(faces, 16, L * 0.025, up_only=True)
        for side in (-1, 1):
            s.plate([(-L * 0.2, side * L * 0.12, 0), (-L * 0.5, side * L * 0.3, L * 0.04), (-L * 0.5, side * L * 0.3, -L * 0.04), (-L * 0.5, side * L * 0.2, 0),
                     (-L * 0.3, side * L * 0.2, L * 0.05)], colour=ACCENT)
            s.box((-L * 0.4, side * L * 0.25, -L * 0.04), (L * 0.2, L * 0.03, L * 0.01), colour=COOL)
            s.socket("WPN_Beam", (L * 0.1, side * L * 0.12, L * 0.05))
        s.box((L * 0.1, 0, L * 0.07), (L * 0.4, L * 0.04, L * 0.02), colour=COOL)
        s.socket("WPN_Torp", (L * 0.48, 0, 0))
    s.socket("ENG", (-L * 0.5, 0, 0))
    return s


def build_borg(s, p):
    L = p["length"]
    arch = p["arch"]
    if arch == "cube":
        h = L * 0.5
        s.box((0, 0, 0), (L * 0.98, L * 0.98, L * 0.98))
        # Layered superstructure: recessed plates and conduits on every face, green glow seams.
        r = s.rng
        for axis in range(3):
            for sgn in (-1, 1):
                for _ in range(26):
                    u, v = r.range(-h * 0.9, h * 0.9), r.range(-h * 0.9, h * 0.9)
                    size = [r.range(L * 0.04, L * 0.18), r.range(L * 0.04, L * 0.18), r.range(L * 0.01, L * 0.04)]
                    c = [0, 0, 0]
                    c[axis] = sgn * (h * 0.98 + size[2] * 0.3)
                    others = [i for i in range(3) if i != axis]
                    c[others[0]], c[others[1]] = u, v
                    dims = [0, 0, 0]
                    dims[axis] = size[2]
                    dims[others[0]], dims[others[1]] = size[0], size[1]
                    s.box(tuple(c), tuple(dims), colour=COOL if r.next() < 0.18 else (ACCENT if r.next() < 0.3 else HULL))
        for k in range(6):
            s.socket("WPN_Beam", [(h, 0, 0), (-h, 0, 0), (0, h, 0), (0, -h, 0), (0, 0, h), (0, 0, -h)][k])
        s.socket("WPN_Torp", (h, h * 0.5, 0))
    elif arch == "sphere":
        s.ellipsoid((0, 0, 0), (L * 0.5, L * 0.5, L * 0.5), segments=32, rings=16)
        faces = [f for f in s.bm.faces]
        s.greebles(faces, 90, L * 0.08, colour=HULL)
        s.greebles(faces, 20, L * 0.05, colour=COOL)
        s.socket("WPN_Beam", (L * 0.5, 0, 0))
        s.socket("WPN_Torp", (L * 0.45, L * 0.1, 0))
    else:  # diamond / probe / scout
        s.plate([(L * 0.5, 0, 0), (-L * 0.5, 0, 0), (0, L * 0.3, 0), (0, -L * 0.3, 0), (0, 0, L * 0.3), (0, 0, -L * 0.3)])
        s.greebles([f for f in s.bm.faces], 40, L * 0.07)
        s.greebles([f for f in s.bm.faces], 10, L * 0.05, colour=COOL)
        s.socket("WPN_Beam", (L * 0.5, 0, 0))
    s.socket("ENG", (-L * 0.5, 0, 0))
    return s


def build_station(s, p, dome=False):
    L = p["length"]
    arch = p.get("arch", "station")
    if arch == "outpost":
        s.lathe([(-L * 0.5, 0), (-L * 0.45, L * 0.2), (-L * 0.1, L * 0.25), (L * 0.2, L * 0.12), (L * 0.5, 0.0)],
                Matrix.Rotation(-math.pi / 2, 4, "Y"), 16)
        s.box((0, 0, 0), (L * 0.9, L * 0.06, L * 0.06))
        s.box((0, 0, 0), (L * 0.06, L * 0.9, L * 0.06), colour=WINDOWS)
    else:  # mushroom starbase / spacedock: dome, stem, docking ring
        R = L * 0.5
        s.lathe([(R * 0.95, 0.0), (R * 0.9, R * 0.35), (R * 0.72, R * 0.8), (R * 0.55, R * 0.98), (R * 0.4, R), (R * 0.3, R * 0.9),
                 (R * 0.25, R * 0.3), (-R * 0.5, R * 0.2), (-R * 0.7, R * 0.35), (-R * 0.8, R * 0.3), (-R * 0.95, 0.0)],
                Matrix.Rotation(-math.pi / 2, 4, "Y"), 48,
                colour_fn=lambda i, n, j, ns: WINDOWS if i in (2, 4, 7) else (ACCENT if i == 3 else HULL))
        s.arc_sweep(R * 0.75, 0, 2 * math.pi, (R * 0.06, R * 0.05, 10), Matrix.Translation((0, 0, -R * 0.2)), steps=48, colour=WINDOWS)
        for k in range(8):
            a = 2 * math.pi * k / 8
            s.box((R * 0.62 * math.cos(a), R * 0.62 * math.sin(a), -R * 0.2), (R * 0.26, R * 0.03, R * 0.03))
            s.socket("WPN_Beam", (R * 0.95 * math.cos(a), R * 0.95 * math.sin(a), R * 0.3))
        s.socket("WPN_Torp", (0, 0, R * 0.9))
    return s


def build_alien(s, p):
    """Generic minor-race designs chosen by the key hash, so each race keeps a consistent look."""
    L = p["length"]
    style = p.get("style") or s.rng.pick(["arrow", "ring", "organic", "blade", "crab"])
    r = s.rng
    if style == "arrow":
        w = L * r.range(0.3, 0.6)
        s.plate([(L * 0.5, 0, 0), (-L * 0.5, w * 0.5, L * 0.04), (-L * 0.5, -w * 0.5, L * 0.04), (-L * 0.5, w * 0.4, -L * 0.05), (-L * 0.5, -w * 0.4, -L * 0.05),
                 (-L * 0.2, 0, L * 0.12)])
        s.box((-L * 0.48, 0, 0), (L * 0.04, w * 0.6, L * 0.05), colour=WARM)
        s.socket("WPN_Beam", (L * 0.45, 0, 0))
    elif style == "ring":
        s.arc_sweep(L * 0.38, 0, 2 * math.pi, (L * 0.07, L * 0.05, 12), Matrix(), steps=40)
        s.ellipsoid((0, 0, 0), (L * 0.2, L * 0.2, L * 0.12), colour=COOL)
        for k in range(4):
            a = math.pi * 0.5 * k
            s.box((L * 0.19 * math.cos(a), L * 0.19 * math.sin(a), 0), (L * 0.3 if k % 2 == 0 else L * 0.04, L * 0.04 if k % 2 == 0 else L * 0.3, L * 0.03))
        s.socket("WPN_Beam", (L * 0.45, 0, 0))
    elif style == "organic":
        for k in range(5):
            s.ellipsoid((L * (0.35 - k * 0.17), r.range(-L * 0.05, L * 0.05), r.range(-L * 0.04, L * 0.04)),
                        (L * 0.12, L * r.range(0.08, 0.16), L * r.range(0.06, 0.12)), colour=HULL if k % 2 else ACCENT)
        s.ellipsoid((L * 0.45, 0, 0), (L * 0.05, L * 0.05, L * 0.05), colour=COOL)
        s.socket("WPN_Beam", (L * 0.5, 0, 0))
    elif style == "blade":
        s.plate([(L * 0.5, 0, 0), (-L * 0.5, 0, L * 0.25), (-L * 0.5, 0, -L * 0.25), (-L * 0.3, L * 0.06, 0), (-L * 0.3, -L * 0.06, 0)])
        for side in (-1, 1):
            s.wing((0, side * L * 0.05, 0), (-L * 0.4, side * L * 0.05, 0), (-L * 0.3, side * L * 0.4, 0), (-L * 0.45, side * L * 0.4, 0), L * 0.03, L * 0.01, colour=ACCENT)
        s.box((-L * 0.48, 0, 0), (L * 0.04, L * 0.1, L * 0.3), colour=WARM)
        s.socket("WPN_Beam", (L * 0.5, 0, 0))
    else:  # crab
        s.ellipsoid((0, 0, 0), (L * 0.3, L * 0.25, L * 0.1))
        for side in (-1, 1):
            s.tube((L * 0.1, side * L * 0.2, 0), (L * 0.5, side * L * 0.1, 0), L * 0.05, L * 0.02, segments=10)
            s.tube((-L * 0.1, side * L * 0.2, 0), (-L * 0.5, side * L * 0.35, -L * 0.05), L * 0.05, L * 0.03, segments=10)
            s.socket("WPN_Pulse", (L * 0.5, side * L * 0.1, 0))
        s.box((-L * 0.25, 0, L * 0.08), (L * 0.2, L * 0.2, L * 0.03), colour=COOL)
    s.greebles([f for f in s.bm.faces], 12, L * 0.03)
    s.socket("ENG", (-L * 0.5, 0, 0))
    s.socket("WPN_Torp", (L * 0.4, 0, -L * 0.02))
    return s


BUILDERS = {"fed": build_fed, "kli": build_kli, "rom": build_rom, "car": build_car, "fer": build_fer, "dom": build_dom, "borg": build_borg}

# ----------------------------------------------------------------------------------------------------------------
# Spec table: archetype, canon length (m) and shape parameters per key. Unknown keys get a role-based default.
# ----------------------------------------------------------------------------------------------------------------
SPECS = {
    # Federation
    "ship.fed.galaxy": dict(arch="explorer", length=642, saucer=0.3, oval=1.12, span=0.55),
    "ship.fed.galaxy_x": dict(arch="explorer", length=700, saucer=0.28, oval=1.12, span=0.6),
    "ship.fed.sovereign": dict(arch="explorer", length=685, saucer=0.22, oval=1.25, span=0.7, nac_len=0.55),
    "ship.fed.ambassador": dict(arch="explorer", length=526, saucer=0.27, span=0.6),
    "ship.fed.luna": dict(arch="explorer", length=450, saucer=0.25, span=0.65),
    "ship.fed.olympic": dict(arch="science", length=310, saucer=0.32),
    "ship.fed.kelvin_type": dict(arch="cruiser", length=240, saucer=0.2),
    "ship.fed.constitution": dict(arch="cruiser", length=289),
    "ship.fed.constitution_refit": dict(arch="cruiser", length=305, nac_len=0.55),
    "ship.fed.constitution_kelvin": dict(arch="cruiser", length=725, saucer=0.2),
    "ship.fed.excelsior": dict(arch="cruiser", length=467, saucer=0.2, nac_len=0.55),
    "ship.fed.vengeance": dict(arch="cruiser", length=965, saucer=0.18, nac_len=0.52),
    "ship.fed.edward": dict(arch="cruiser", length=280),
    "ship.fed.cheyenne": dict(arch="frigate", length=359, nacelles=4),
    "ship.fed.constellation": dict(arch="frigate", length=310, nacelles=4, saucer=0.4),
    "ship.fed.challenger": dict(arch="frigate", length=325, nacelles=4),
    "ship.fed.miranda": dict(arch="frigate", length=243, rollbar=True),
    "ship.fed.soyuz": dict(arch="frigate", length=260, rollbar=True),
    "ship.fed.centaur": dict(arch="frigate", length=237, nac_up=True),
    "ship.fed.freedom": dict(arch="frigate", length=240, nacelles=2),
    "ship.fed.new_orleans": dict(arch="frigate", length=348, nacelles=4),
    "ship.fed.curry": dict(arch="frigate", length=295, rollbar=True),
    "ship.fed.springfield": dict(arch="frigate", length=365),
    "ship.fed.rhode_island": dict(arch="science", length=345, saucer=0.3, oval=1.2),
    "ship.fed.defiant": dict(arch="escort", length=120, width=0.78, height=0.24),
    "ship.fed.saber": dict(arch="escort", length=190, width=0.95, height=0.14),
    "ship.fed.yeager": dict(arch="escort", length=150, width=0.8),
    "ship.fed.norway": dict(arch="escort", length=365, width=0.7, height=0.16),
    "ship.fed.steamrunner": dict(arch="escort", length=240, width=0.85, height=0.14),
    "ship.fed.akira": dict(arch="escort", length=464, width=0.85, height=0.15, pod=True),
    "ship.fed.prometheus": dict(arch="escort", length=415, width=0.6, height=0.22),
    "ship.fed.peregrine": dict(arch="escort", length=30, width=0.6),
    "ship.fed.london": dict(arch="escort", length=350, width=0.75),
    "ship.fed.intrepid": dict(arch="science", length=344, saucer=0.3, oval=1.35, nac_up=True),
    "ship.fed.nova": dict(arch="science", length=165, saucer=0.33),
    "ship.fed.nebula": dict(arch="science", length=442, saucer=0.33, oval=1.1, pod=True),
    "ship.fed.oberth": dict(arch="science", length=120, oberth=True, saucer=0.3),
    "ship.fed.daedalus": dict(arch="science", length=105, oberth=True, saucer=0.32),
    "ship.fed.nx": dict(arch="science", length=225, saucer=0.36, oval=1.0),
    "ship.fed.danube": dict(arch="runabout", length=23),
    "ship.fed.yclass": dict(arch="runabout", length=60),
    "ship.fed.sydney": dict(arch="runabout", length=97),
    "ship.fed.raven": dict(arch="runabout", length=36),
    "ship.fed.wells": dict(arch="science", length=450, saucer=0.28, oval=1.1),
    "ship.fed.ranger": dict(arch="frigate", length=230),
    "ship.fed.starbase": dict(arch="station", length=1200, builder="station"),
    "ship.fed.spacedock": dict(arch="station", length=1800, builder="station"),
    "ship.fed.k7": dict(arch="outpost", length=500, builder="station"),
    "ship.fed.outpost": dict(arch="outpost", length=300, builder="station"),
    # Klingon
    "ship.kli.bop": dict(arch="bop", length=110),
    "ship.kli.brel": dict(arch="bop", length=110),
    "ship.kli.kvort": dict(arch="bop", length=350, span=0.72),
    "ship.kli.bop_ent": dict(arch="bop", length=145, dihedral=0.0),
    "ship.kli.raptor": dict(arch="bop", length=90, span=0.5, dihedral=-0.1),
    "ship.kli.mayduj": dict(arch="bop", length=260, span=0.55),
    "ship.kli.ktinga": dict(arch="battlecruiser", length=350, neck=0.33, aft_width=0.32),
    "ship.kli.d7": dict(arch="battlecruiser", length=228, neck=0.35, aft_width=0.34),
    "ship.kli.d5": dict(arch="battlecruiser", length=190, neck=0.3),
    "ship.kli.d4": dict(arch="battlecruiser", length=180, neck=0.25),
    "ship.kli.d12": dict(arch="bop", length=210, span=0.7, dihedral=-0.2),
    "ship.kli.vorcha": dict(arch="battlecruiser", length=481, neck=0.18, head=0.14, aft_width=0.45, span=0.55, wing_drop=0.1),
    "ship.kli.neghvar": dict(arch="battlecruiser", length=682, neck=0.14, head=0.15, aft_width=0.5, span=0.6, wing_drop=0.35),
    "ship.kli.neghtev": dict(arch="battlecruiser", length=720, neck=0.16, head=0.15, aft_width=0.5),
    "ship.kli.qang": dict(arch="battlecruiser", length=350, neck=0.28),
    "ship.kli.liwi": dict(arch="battlecruiser", length=260, neck=0.3),
    "ship.kli.charghwi": dict(arch="battlecruiser", length=500, neck=0.2, aft_width=0.42),
    "ship.kli.starbase": dict(arch="station", length=1100, builder="station"),
    "ship.kli.outpost": dict(arch="outpost", length=300, builder="station"),
    # Romulan
    "ship.rom.dderidex": dict(arch="warbird", length=1041),
    "ship.rom.ddredar": dict(arch="warbird", length=900),
    "ship.rom.valdore": dict(arch="bird", length=843, span=1.0, sweep=0.3),
    "ship.rom.mogai": dict(arch="bird", length=500, span=0.95),
    "ship.rom.norexan": dict(arch="bird", length=310, span=0.8, sweep=0.2),
    "ship.rom.bop": dict(arch="bird", length=180, span=0.9),
    "ship.rom.bop_tos": dict(arch="bird", length=131, span=1.0, sweep=0.15, lift=0.0),
    "ship.rom.bop_ent": dict(arch="bird", length=200, span=0.7),
    "ship.rom.scout": dict(arch="bird", length=50, span=0.6),
    "ship.rom.science": dict(arch="bird", length=250, span=0.7, sweep=0.2),
    "ship.rom.drone": dict(arch="bird", length=120, span=0.6),
    "ship.rom.narada": dict(arch="alien", length=8000, style="blade", builder="alien"),
    "ship.rom.rtan": dict(arch="bird", length=150),
    "ship.rom.dretex": dict(arch="bird", length=240),
    "ship.rom.dridren": dict(arch="bird", length=330),
    "ship.rom.drenet": dict(arch="bird", length=420),
    "ship.rom.rdaran": dict(arch="warbird", length=700),
    "ship.rom.rderex": dict(arch="warbird", length=600),
    "ship.rom.starbase": dict(arch="station", length=1100, builder="station"),
    "ship.rom.outpost": dict(arch="outpost", length=300, builder="station"),
    # Cardassian
    "ship.car.galor": dict(arch="galor", length=371),
    "ship.car.keldon": dict(arch="galor", length=372, keldon=True),
    "ship.car.kraxon": dict(arch="galor", length=280, width=0.5),
    "ship.car.ranol": dict(arch="galor", length=200),
    "ship.car.netel": dict(arch="galor", length=300),
    "ship.car.nerok": dict(arch="galor", length=420, keldon=True),
    "ship.car.toran": dict(arch="galor", length=500, keldon=True),
    "ship.car.neterok": dict(arch="galor", length=450),
    "ship.car.telok": dict(arch="galor", length=520, keldon=True),
    "ship.car.hideki": dict(arch="scarab", length=70),
    "ship.car.scout": dict(arch="scarab", length=60),
    "ship.car.groumall": dict(arch="scarab", length=200),
    "ship.car.atr4107": dict(arch="scarab", length=150),
    "ship.car.platform": dict(arch="outpost", length=120, builder="station"),
    "ship.car.nor": dict(arch="station", length=1450, builder="station"),
    "ship.car.starbase": dict(arch="station", length=1100, builder="station"),
    "ship.car.outpost": dict(arch="outpost", length=300, builder="station"),
    # Ferengi
    "ship.fer.dkora": dict(arch="marauder", length=350),
    "ship.fer.nagus": dict(arch="marauder", length=600),
    "ship.fer.ooron": dict(arch="marauder", length=250),
    "ship.fer.tomax": dict(arch="marauder", length=300),
    "ship.fer.ngort": dict(arch="marauder", length=400),
    "ship.fer.bronta": dict(arch="marauder", length=450),
    "ship.fer.glantor": dict(arch="marauder", length=500),
    "ship.fer.goront": dict(arch="marauder", length=550),
    "ship.fer.tokorn": dict(arch="pod", length=120),
    "ship.fer.shuttle": dict(arch="pod", length=15),
    "ship.fer.scout": dict(arch="pod", length=60),
    "ship.fer.pod": dict(arch="pod", length=45),
    "ship.fer.starbase": dict(arch="station", length=1100, builder="station"),
    "ship.fer.outpost": dict(arch="outpost", length=300, builder="station"),
    # Dominion / Borg
    "ship.dom.attack_ship": dict(arch="attack", length=68),
    "ship.dom.battlecruiser": dict(arch="battle", length=680),
    "ship.dom.battleship": dict(arch="battle", length=1000),
    "ship.borg.cube": dict(arch="cube", length=3036),
    "ship.borg.tactical_cube": dict(arch="cube", length=3100),
    "ship.borg.sphere": dict(arch="sphere", length=600),
    "ship.borg.diamond": dict(arch="diamond", length=1000),
    "ship.borg.probe": dict(arch="diamond", length=400),
    "ship.borg.scout": dict(arch="diamond", length=100),
}

ROLE_LENGTH = {"Scout": 90, "Raider": 120, "Escort": 150, "Frigate": 250, "Destroyer": 250, "Light Cruiser": 300, "Cruiser": 350,
               "Strike Cruiser": 380, "Heavy Cruiser": 450, "Battlecruiser": 550, "Battleship": 700, "Dreadnought": 900, "Carrier": 800,
               "Science Vessel": 300, "Transport": 150, "Special": 500, "Station": 1000}


def spec_for(key, display):
    if key in SPECS:
        spec = dict(SPECS[key])
    else:
        role = display.split("(")[-1].split(",")[0].strip() if "(" in display else "Cruiser"
        spec = dict(arch="alien", builder="alien", length=ROLE_LENGTH.get(role, 350))
    fac = key.split(".")[1]
    if "builder" not in spec:
        spec["builder"] = fac if fac in BUILDERS else "alien"
    return spec


class Varied(dict):
    """Spec whose numeric proportions (explicit or builder defaults) vary by up to +/-12% per class key, so classes
    sharing an archetype (every Ferengi marauder, every Romulan bird) get their own silhouette. Length is exact."""

    def __init__(self, key, spec):
        super().__init__(spec)
        self.key = key

    def get(self, name, default=None):
        value = super().get(name, default)
        if name == "length" or not isinstance(value, float):  # counts (nacelles=2) and flags stay exact
            return value
        return value * (0.88 + 0.24 * (key_seed(self.key + "/" + name) % 10000) / 10000.0)

    def __getitem__(self, name):
        return self.get(name) if name != "length" and name in self else super().__getitem__(name)


def build(key, spec):
    s = Ship(key, key_seed(key))
    spec = Varied(key, spec)
    builder = spec["builder"]
    if builder == "station":
        build_station(s, spec)
    elif builder == "alien":
        build_alien(s, spec)
    else:
        BUILDERS[builder](s, spec)
    return s


# ----------------------------------------------------------------------------------------------------------------
# Output
# ----------------------------------------------------------------------------------------------------------------
def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def normalise(ob, length):
    """Recentre on the bounds and scale so the longest extent is the canon length; returns the transform."""
    bb = [Vector(c) for c in ob.bound_box]
    lo = Vector((min(v.x for v in bb), min(v.y for v in bb), min(v.z for v in bb)))
    hi = Vector((max(v.x for v in bb), max(v.y for v in bb), max(v.z for v in bb)))
    centre = (lo + hi) / 2
    extent = max(hi - lo)
    k = length / extent if extent > 0 else 1.0
    m = Matrix.Scale(k, 4) @ Matrix.Translation(-centre)
    ob.data.transform(m)
    return m


def export(key, spec, out_dir, preview_dir):
    reset()
    s = build(key, spec)
    ob = s.to_object(key.replace(".", "_"), spec["length"])
    m = normalise(ob, spec["length"])
    ob.data.update()
    # Finish: narrow angle-limited bevels with hardened normals so plate edges catch the light, like machined panels.
    bpy.context.view_layer.objects.active = ob
    bev = ob.modifiers.new("Bevel", "BEVEL")
    bev.limit_method = "ANGLE"
    bev.angle_limit = math.radians(32)
    bev.width = spec["length"] * 0.0025
    bev.segments = 2
    bev.harden_normals = True
    ob.modifiers.new("WeightedNormal", "WEIGHTED_NORMAL").keep_sharp = True
    bpy.ops.object.modifier_apply(modifier="Bevel")
    bpy.ops.object.modifier_apply(modifier="WeightedNormal")
    tris = sum(len(p.vertices) - 2 for p in ob.data.polygons)
    for name, pos in s.sockets:
        e = bpy.data.objects.new("SOCKET_" + name, None)
        e.location = m @ pos
        e.empty_display_size = spec["length"] * 0.01
        bpy.context.scene.collection.objects.link(e)
        e.parent = ob
    fac, slug = key.split(".")[1], key.split(".", 2)[2]
    path = os.path.join(out_dir, fac, slug + ".fbx")
    os.makedirs(os.path.dirname(path), exist_ok=True)
    bpy.ops.export_scene.fbx(filepath=path, use_selection=False, apply_unit_scale=True, global_scale=1.0, axis_forward="-Y", axis_up="Z",
                             object_types={"MESH", "EMPTY"}, mesh_smooth_type="FACE", colors_type="LINEAR", use_mesh_modifiers=True,
                             add_leaf_bones=False, bake_space_transform=False)
    if preview_dir:
        render_preview(ob, spec["length"], os.path.join(preview_dir, key + ".png"))
    return {"key": key, "file": os.path.relpath(path, out_dir).replace("\\", "/"), "lengthMeters": spec["length"], "triangles": tris,
            "archetype": spec["arch"], "sockets": [n for n, _ in s.sockets]}


def render_preview(ob, length, path):
    scene = bpy.context.scene
    mat = bpy.data.materials.new("Preview")
    mat.use_nodes = True
    nt = mat.node_tree
    bsdf = nt.nodes.get("Principled BSDF")
    attr = nt.nodes.new("ShaderNodeVertexColor")
    attr.layer_name = "Mask"
    sep = nt.nodes.new("ShaderNodeSeparateColor")
    nt.links.new(attr.outputs["Color"], sep.inputs["Color"])
    bsdf.inputs["Base Color"].default_value = (0.55, 0.57, 0.6, 1)
    bsdf.inputs["Metallic"].default_value = 0.6
    bsdf.inputs["Roughness"].default_value = 0.4
    emit = nt.nodes.new("ShaderNodeCombineColor")
    nt.links.new(sep.outputs["Green"], emit.inputs["Red"])
    nt.links.new(sep.outputs["Red"], emit.inputs["Green"])
    nt.links.new(sep.outputs["Blue"], emit.inputs["Blue"])
    nt.links.new(emit.outputs["Color"], bsdf.inputs["Emission Color"])
    bsdf.inputs["Emission Strength"].default_value = 4.0
    ob.data.materials.append(mat)
    cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
    scene.collection.objects.link(cam)
    cam.location = (length * 0.95, -length * 1.25, length * 0.55)
    cam.rotation_euler = (math.radians(68), 0, math.radians(37))
    cam.data.lens = 50
    cam.data.clip_start = length * 0.01
    cam.data.clip_end = length * 20
    scene.camera = cam
    for rot, energy in (((math.radians(40), 0, math.radians(30)), 4.0), ((math.radians(-60), 0, math.radians(200)), 1.5)):
        light = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
        light.data.energy = energy
        light.rotation_euler = rot
        scene.collection.objects.link(light)
    world = bpy.data.worlds.new("W")
    world.color = (0.02, 0.02, 0.03)
    scene.world = world
    scene.render.engine = "BLENDER_EEVEE_NEXT" if "BLENDER_EEVEE_NEXT" in [e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items] else "BLENDER_EEVEE"
    scene.render.resolution_x = 480
    scene.render.resolution_y = 320
    scene.render.filepath = path
    bpy.ops.render.render(write_still=True)


def arg(name, default=None):
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    if name in argv:
        i = argv.index(name)
        return argv[i + 1] if i + 1 < len(argv) and not argv[i + 1].startswith("--") else True
    return default


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    out_dir = os.path.abspath(arg("--out", os.path.join(here, "..", "ArtSource", "Generated", "Ships")))
    manifest = json.load(open(os.path.join(here, "..", "..", "assets", "manifest.json"), encoding="utf-8-sig"))
    keys = [(k, v.get("displayName", "")) for k, v in manifest["assets"].items() if k.startswith("ship.")]
    patterns = arg("--keys")
    if isinstance(patterns, str):
        pats = patterns.split(",")
        keys = [(k, d) for k, d in keys if any(fnmatch.fnmatch(k, p) for p in pats)]
    preview_dir = os.path.join(out_dir, "previews") if arg("--preview") else None
    if preview_dir:
        os.makedirs(preview_dir, exist_ok=True)
    results = []
    for key, display in keys:
        try:
            r = export(key, spec_for(key, display), out_dir, preview_dir)
            results.append(r)
            print("OK  %-32s %-12s %6d tris  %d sockets" % (key, r["archetype"], r["triangles"], len(r["sockets"])))
        except Exception as e:  # keep going: one bad spec must not stop the batch
            import traceback
            traceback.print_exc()
            print("ERR %s  %s" % (key, e))
    listing = os.path.join(out_dir, "generated-ships.json")
    existing = {}
    if os.path.exists(listing) and patterns:
        existing = {e["key"]: e for e in json.load(open(listing, encoding="utf-8"))["ships"]}
    for r in results:
        existing[r["key"]] = r
    with open(listing, "w", encoding="utf-8") as f:
        json.dump({"ships": sorted(existing.values(), key=lambda e: e["key"])}, f, indent=1)
    print("generated %d/%d" % (len(results), len(keys)))


main()
