"""Kitbash recipes for the hulls the library lacks (L9). Each recipe edits a base hull from the Starhelm/Bridge Commander
library (private, never committed) with kb_core.Kit: fractional-box selections, region moves and scales, duplicated parts,
and primitives that take the hull's own paint. 'length' is the canon length (asset-manifest length_m).

Canon / BOTF look each one aims at (the sheets are the judge):
  edward     colony ship: a Nebula saucer carrying two huge colony cylinders and a dorsal cargo block
  london     long-range cruiser: a Constellation widened and stretched, big saucer, ventral tactical pod, dorsal dome
  sydney     transport: an Oberth saucer with a cargo-container spine and four containers
  soyuz      frigate (TNG 'Cause and Effect' Bozeman): Excelsior-pattern slender hull, small saucer, nacelles raised on pylons
  vengeance  dreadnought (Star Trek Into Darkness): a Sovereign made massive: wide flattened wedge, thick dorsal and ventral keel
  wells      special: a Saber with a large temporal ring round the aft hull
  yclass     utility transport: a Danube with a cargo module and two outboard pods
  k7         station (TOS 'Trouble with Tribbles'): a Starbase column with its saucer cut away, a docking ring, four arms, a dome
  charghwi   Klingon heavy cruiser: a Vor'cha with broadened wings, a dorsal gun deck and a ventral keel
  d12        Klingon destroyer: a K't'inga with the neck shortened, a stubby body and twin forward cannons
  liwi       Klingon cruiser: a K't'inga with a heavier command pod, forward sponsons and a second boom
  mayduj     Klingon bulk carrier: a Kvort with two big cargo cylinders slung under the wings
  neghtev    Klingon battleship: a Negh'var lengthened and thickened with a dorsal gun block and keel
  qang       Klingon heavy cruiser: a Kvort with a ram prow and side sponsons
"""
import math

from mathutils import Matrix, Vector

from kb_peregrine import PEREGRINE_SOCKETS, peregrine_object, peregrine_paint  # noqa: F401


def torus(k, c, R, r, axis='Z', segs=36, tube=12, slot=None):
    """Ring of tube radius r and ring radius R around the axis through c."""
    import bmesh
    rings = []
    rot = {'Z': Matrix.Identity(4), 'X': Matrix.Rotation(math.pi / 2, 4, 'Y'), 'Y': Matrix.Rotation(math.pi / 2, 4, 'X')}[axis]
    vs_all = []
    for i in range(segs):
        a = 2 * math.pi * i / segs
        ring = []
        for j in range(tube):
            b = 2 * math.pi * j / tube
            p = Vector(((R + r * math.cos(b)) * math.cos(a), (R + r * math.cos(b)) * math.sin(a), r * math.sin(b)))
            p = rot @ p + Vector(c)
            ring.append(k.bm.verts.new(p))
        rings.append(ring)
        vs_all += ring
    faces = []
    for i in range(segs):
        a, b = rings[i], rings[(i + 1) % segs]
        for j in range(tube):
            faces.append(k.bm.faces.new((a[j], a[(j + 1) % tube], b[(j + 1) % tube], b[j])))
    k._paint(faces, slot)
    return vs_all


# --------------------------------------------------------------------------------------------------------------------
# Federation

def edward(k):
    L, W, H = k.ext
    k.stretch(1.0, 1.06, 1.0)
    L, W, H = k.ext
    for s in (1, -1):
        k.cyl(k.at(0.30, 0.5 + s * 0.15, 0.30), radius=0.085 * W, length=0.46 * L, axis='X', r2=0.07 * W)
        k.sphere(k.at(0.075, 0.5 + s * 0.15, 0.30), 0.085 * W, (1.0, 1, 1))
    k.box_prim(k.at(0.24, 0.5, 0.80), (0.30 * L, 0.20 * W, 0.12 * H))
    k.box_prim(k.at(0.10, 0.5, 0.70), (0.12 * L, 0.30 * W, 0.10 * H))


def london(k):
    k.stretch(1.12, 1.0, 1.0)
    L, W, H = k.ext
    k.scale(k.box(x=(0.45, 1.0)), k.at(0.7, 0.5, 0.5), (1.0, 1.14, 1.0))
    k.move(k.box(x=(0.0, 0.5), ay=(0.55, 1.0)), (0, 0, 0))
    k.box_prim(k.at(0.70, 0.5, 0.18), (0.26 * L, 0.14 * W, 0.07 * H))
    k.sphere(k.at(0.68, 0.5, 0.98), 0.06 * W, (1.3, 1, 0.8))
    k.cyl(k.at(0.97, 0.5, 0.5), radius=0.02 * W, length=0.1 * L, axis='X', r2=0.005 * W)


def sydney(k):
    k.stretch(1.0, 1.0, 1.0)
    L, W, H = k.ext
    k.box_prim(k.at(0.25, 0.5, 0.78), (0.55 * L, 0.20 * W, 0.16 * H))
    for i, fx in enumerate((0.12, 0.28, 0.44)):
        for s in (1, -1):
            k.box_prim(k.at(fx, 0.5 + s * 0.17, 0.78), (0.14 * L, 0.14 * W, 0.20 * H))
    k.box_prim(k.at(0.0, 0.5, 0.5), (0.12 * L, 0.35 * W, 0.40 * H))


def soyuz(k):
    k.stretch(1.0, 1.0, 1.0)
    L, W, H = k.ext
    k.scale(k.box(x=(0.5, 1.0)), k.at(0.75, 0.5, 0.5), (0.9, 0.9, 1.0))
    # raise the outboard nacelle masses on pylons
    nac = k.box(x=(0.0, 0.55), ay=(0.55, 1.0))
    k.move(nac, (0, 0, 0.12 * H))
    for s in (1, -1):
        k.box_prim(k.at(0.30, 0.5 + s * 0.30, 0.55), (0.18 * L, 0.03 * W, 0.20 * H), rot=(s * 0.25, 0, 0))


def vengeance(k):
    k.stretch(1.0, 1.0, 1.0)
    L, W, H = k.ext
    k.warp(lambda co, f: Vector((co.x, co.y * (2.3 + 0.5 * f[0]), co.z * 1.9)))
    k.refresh()
    L, W, H = k.ext
    k.box_prim(k.at(0.35, 0.5, 0.95), (0.55 * L, 0.22 * W, 0.10 * H))
    k.box_prim(k.at(0.45, 0.5, 0.05), (0.50 * L, 0.18 * W, 0.12 * H))
    for s in (1, -1):
        k.box_prim(k.at(0.55, 0.5 + s * 0.30, 0.55), (0.40 * L, 0.10 * W, 0.12 * H), rot=(0, 0, s * 0.10))
    k.cyl(k.at(0.96, 0.5, 0.5), radius=0.05 * W, length=0.10 * L, axis='X', r2=0.02 * W)


def wells(k):
    k.stretch(1.0, 1.0, 1.0)
    L, W, H = k.ext
    torus(k, k.at(0.30, 0.5, 0.5), R=0.40 * W, r=0.045 * W, axis='X')
    for a in range(4):
        ang = a * math.pi / 2 + math.pi / 4
        k.box_prim(k.at(0.30, 0.5 + 0.2 * W / W * math.cos(ang) * 0.9 * (W / W), 0.5 + 0.2 * math.sin(ang) * 0.9 * (W / H)), (0.04 * L, 0.20 * W, 0.04 * H), rot=(ang, 0, 0))
    k.cyl(k.at(0.0, 0.5, 0.5), radius=0.10 * W, length=0.08 * L, axis='X', r2=0.12 * W)


def yclass(k):
    k.stretch(1.35, 1.1, 1.0)
    L, W, H = k.ext
    k.box_prim(k.at(0.35, 0.5, 0.78), (0.5 * L, 0.5 * W, 0.34 * H))
    for s in (1, -1):
        k.cyl(k.at(0.30, 0.5 + s * 0.38, 0.40), radius=0.10 * W, length=0.5 * L, axis='X', r2=0.08 * W)


def k7(k):
    k.stretch(1.0, 1.0, 1.0)
    L, W, H = k.ext
    k.delete(k.box(z=(0.58, 1.0)))
    L, W, H = k.ext
    torus(k, k.at(0.5, 0.5, 0.62), R=0.42 * W, r=0.045 * W, axis='Z')
    for a in range(4):
        ang = a * math.pi / 2
        k.box_prim(k.at(0.5 + 0.22 * math.cos(ang) * (W / L), 0.5 + 0.22 * math.sin(ang), 0.62), (0.44 * W if a % 2 == 0 else 0.03 * W, 0.03 * W if a % 2 == 0 else 0.44 * W, 0.03 * H), slot=None)
    k.sphere(k.at(0.5, 0.5, 0.70), 0.10 * W, (1, 1, 0.9))
    k.cyl(k.at(0.5, 0.5, 0.82), radius=0.03 * W, length=0.20 * H, axis='Z', r2=0.01 * W)


# --------------------------------------------------------------------------------------------------------------------
# Klingon

def charghwi(k):
    L, W, H = k.ext
    k.scale(k.box(), k.at(0.5, 0.5, 0.5), (1.0, 1.22, 1.0))
    k.refresh()
    L, W, H = k.ext
    k.box_prim(k.at(0.45, 0.5, 0.88), (0.30 * L, 0.18 * W, 0.12 * H))
    k.cyl(k.at(0.50, 0.5, 0.86), radius=0.03 * W, length=0.2 * L, axis='X', r2=0.02 * W)
    k.box_prim(k.at(0.40, 0.5, 0.12), (0.35 * L, 0.10 * W, 0.10 * H))


def d12(k):
    L, W, H = k.ext
    def squash(co, f):
        x = co.x
        if f[0] > 0.45:
            x = co.x - (f[0] - 0.45) * 0.32 * L
        return Vector((x, co.y * 0.9, co.z))
    k.warp(squash)
    k.refresh()
    L, W, H = k.ext
    for s in (1, -1):
        k.cyl(k.at(0.80, 0.5 + s * 0.09, 0.45), radius=0.025 * W, length=0.28 * L, axis='X', r2=0.018 * W)
    k.box_prim(k.at(0.35, 0.5, 0.55), (0.28 * L, 0.16 * W, 0.10 * H))


def liwi(k):
    L, W, H = k.ext
    k.scale(k.box(), k.at(0.5, 0.5, 0.5), (1.0, 1.14, 1.0))
    k.refresh()
    L, W, H = k.ext
    k.sphere(k.at(0.93, 0.5, 0.55), 0.12 * W, (1.4, 1.0, 0.8))
    for s in (1, -1):
        k.box_prim(k.at(0.55, 0.5 + s * 0.12, 0.40), (0.24 * L, 0.06 * W, 0.08 * H))
    k.cyl(k.at(0.25, 0.5, 0.80), radius=0.03 * W, length=0.36 * L, axis='X', r2=0.02 * W)


def mayduj(k):
    L, W, H = k.ext
    for s in (1, -1):
        k.cyl(k.at(0.42, 0.5 + s * 0.30, 0.25), radius=0.09 * W, length=0.52 * L, axis='X', r2=0.07 * W)
        k.sphere(k.at(0.16, 0.5 + s * 0.30, 0.25), 0.09 * W, (1, 1, 1))
        k.box_prim(k.at(0.42, 0.5 + s * 0.20, 0.38), (0.10 * L, 0.10 * W, 0.04 * H))
    k.box_prim(k.at(0.20, 0.5, 0.65), (0.22 * L, 0.18 * W, 0.12 * H))


def neghtev(k):
    k.stretch(1.06, 1.0, 1.25)
    L, W, H = k.ext
    k.box_prim(k.at(0.40, 0.5, 0.92), (0.30 * L, 0.14 * W, 0.12 * H))
    k.box_prim(k.at(0.45, 0.5, 0.10), (0.34 * L, 0.10 * W, 0.12 * H))
    for s in (1, -1):
        k.cyl(k.at(0.62, 0.5 + s * 0.30, 0.55), radius=0.025 * W, length=0.30 * L, axis='X', r2=0.018 * W)


def qang(k):
    L, W, H = k.ext
    k.cyl(k.at(1.02, 0.5, 0.5), radius=0.06 * W, length=0.16 * L, axis='X', r2=0.0, caps=True)
    for s in (1, -1):
        k.cyl(k.at(0.55, 0.5 + s * 0.20, 0.45), radius=0.045 * W, length=0.36 * L, axis='X', r2=0.03 * W)
    k.box_prim(k.at(0.35, 0.5, 0.80), (0.24 * L, 0.14 * W, 0.10 * H))


RECIPES = {
    'ship.fed.edward': dict(base='ship.fed.nebula', fn=edward, length=280, note='colony ship: Nebula saucer, colony cylinders'),
    'ship.fed.london': dict(base='ship.fed.constellation', fn=london, length=350, note='long-range cruiser'),
    'ship.fed.sydney': dict(base='ship.fed.oberth', fn=sydney, length=97, note='transport with cargo containers'),
    'ship.fed.soyuz': dict(base='ship.fed.excelsior', fn=soyuz, length=260, note='frigate, raised nacelles'),
    'ship.fed.vengeance': dict(base='ship.fed.sovereign', fn=vengeance, length=965, note='dreadnought'),
    'ship.fed.wells': dict(base='ship.fed.saber', fn=wells, length=450, note='special: ring ship'),
    'ship.fed.yclass': dict(base='ship.fed.danube', fn=yclass, length=60, note='utility transport'),
    'ship.fed.k7': dict(base='ship.fed.starbase', fn=k7, length=500, scale_max=True, note='K-7 station: column, ring, arms'),
    'ship.kli.charghwi': dict(base='ship.kli.vorcha', fn=charghwi, length=500, note='heavy cruiser'),
    'ship.kli.d12': dict(base='ship.kli.d7', fn=d12, length=210, note='destroyer'),
    'ship.kli.liwi': dict(base='ship.kli.ktinga', fn=liwi, length=260, note='cruiser'),
    'ship.kli.mayduj': dict(base='ship.kli.kvort', fn=mayduj, length=260, note='bulk carrier'),
    'ship.kli.neghtev': dict(base='ship.kli.neghvar', fn=neghtev, length=720, note='battleship'),
    'ship.kli.qang': dict(base='ship.kli.kvort', fn=qang, length=350, note='ram-prow heavy cruiser'),
}
