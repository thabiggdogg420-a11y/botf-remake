"""Procedural planet texture sets for every planet class in the asset manifest (original work, numpy + Pillow).

    python tools/art/gen_planets.py [--classes m,o,...] [--variants 6] [--workers 3]

Per class and variant (ART-BIBLE section 8, budgets section 12.1), written under LocalAssets/planets/<class>/:
  desktop/v<n>_albedo.png   4096x2048 sRGB, equirectangular, seamless in longitude, no pole pinch (noise is sampled on
                            the unit sphere, not on the flat map)
  desktop/v<n>_normal.png   2048x1024 tangent-space relief
  desktop/v<n>_clouds.png   2048x1024 R8 (classes with an atmosphere)
  desktop/v<n>_night.png    2048x1024 RGB night-side lights / lava glow where the class has them
  web/v<n>_albedo.webp      1024x512 (relief baked into the albedo, no normal), variants 1-4 only
  web/v<n>_clouds.png, v<n>_night.png   512x256
Asteroids: 6 rock meshes (OBJ with UVs) plus a rock albedo and normal instead of a sphere set.
Runs at BelowNormal priority. Output is private (LocalAssets is gitignored).
"""
import argparse
import math
import os
import sys
from concurrent.futures import ProcessPoolExecutor

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import _common as C

OUT = os.path.join(C.LOCAL, 'planets')
CLASSES = ['m', 'o', 'p', 'l', 'h', 'k', 'y', 'gasgiant', 'asteroids']
W, H = 2048, 1024
_TABLE = np.random.default_rng(12345).random(4096).astype(np.float32)


def sphere_dirs(w, h):
    lon = (np.arange(w, dtype=np.float32) + 0.5) / w * 2 * np.pi
    lat = (0.5 - (np.arange(h, dtype=np.float32) + 0.5) / h) * np.pi
    cl, sl = np.cos(lat)[:, None], np.sin(lat)[:, None]
    x = cl * np.cos(lon)[None, :]
    z = cl * np.sin(lon)[None, :]
    y = np.broadcast_to(sl, x.shape)
    return np.stack([x, y, z], -1).astype(np.float32), lat


def _hash(ix, iy, iz, seed):
    h = (ix * 73856093) ^ (iy * 19349663) ^ (iz * 83492791) ^ (seed * 2654435761)
    return _TABLE[h & 4095]


def vnoise(p, freq, seed):
    q = p * freq
    f = np.floor(q)
    t = q - f
    t = t * t * (3 - 2 * t)
    i = f.astype(np.int64)
    out = 0
    for dx in (0, 1):
        wx = t[..., 0] if dx else 1 - t[..., 0]
        for dy in (0, 1):
            wy = t[..., 1] if dy else 1 - t[..., 1]
            for dz in (0, 1):
                wz = t[..., 2] if dz else 1 - t[..., 2]
                out = out + _hash(i[..., 0] + dx, i[..., 1] + dy, i[..., 2] + dz, seed) * wx * wy * wz
    return out


def fbm(p, seed, octaves=8, freq=2.0, gain=0.5, ridged=False):
    tot = np.zeros(p.shape[:-1], np.float32)
    amp, norm = 1.0, 0.0
    for o in range(octaves):
        n = vnoise(p, freq, seed * 31 + o * 7 + 1)
        if ridged:
            n = 1 - np.abs(n * 2 - 1)
        tot += n * amp
        norm += amp
        amp *= gain
        freq *= 2.03
    return tot / norm


def worley(p, freq, seed):
    q = p * freq
    f = np.floor(q).astype(np.int64)
    best = np.full(p.shape[:-1], 9.0, np.float32)
    for dx in (-1, 0, 1):
        for dy in (-1, 0, 1):
            for dz in (-1, 0, 1):
                c = f + np.array([dx, dy, dz])
                jx = _hash(c[..., 0], c[..., 1], c[..., 2], seed)
                jy = _hash(c[..., 0], c[..., 1], c[..., 2], seed + 11)
                jz = _hash(c[..., 0], c[..., 1], c[..., 2], seed + 23)
                d = c + np.stack([jx, jy, jz], -1) - q
                best = np.minimum(best, np.sqrt((d * d).sum(-1)))
    return best


def norm01(a, lo=2, hi=98):
    l, h = np.percentile(a, [lo, hi])
    return np.clip((a - l) / max(h - l, 1e-6), 0, 1)


def ramp(t, stops):
    xs = [s[0] for s in stops]
    return np.stack([np.interp(t, xs, [s[1][c] for s in stops]) for c in range(3)], -1).astype(np.float32)


def hexc(s):
    s = s.lstrip('#')
    return tuple(int(s[i:i + 2], 16) / 255 for i in (0, 2, 4))


def col(s):
    return np.array(hexc(s), np.float32)


def smooth(a, b, x):
    t = np.clip((x - a) / (b - a), 0, 1)
    return t * t * (3 - 2 * t)


def clouds_field(p, seed, cover, sharp=1.0):
    warp = fbm(p, seed + 5, 3, 2.0)[..., None] * 0.6
    c = norm01(fbm(p + warp, seed + 9, 7, 2.5))
    return np.clip((c - (1 - cover)) / max(cover, 1e-3) * sharp, 0, 1)


def night_lights(p, land_mask, seed, density, color=(1.0, 0.78, 0.45)):
    clus = norm01(fbm(p, seed + 40, 5, 3.0))
    spots = norm01(fbm(p, seed + 41, 4, 24.0))
    m = land_mask * smooth(0.55, 0.9, clus) * smooth(1 - density, 1.0, spots)
    return (m[..., None] * np.array(color, np.float32)[None, None, :]).astype(np.float32)


def normal_from_height(h, strength):
    dx = (np.roll(h, -1, 1) - np.roll(h, 1, 1)) * 0.5
    dy = (np.roll(h, -1, 0) - np.roll(h, 1, 0)) * 0.5
    nx, ny, nz = -dx * strength, dy * strength, np.ones_like(h)
    ln = np.sqrt(nx * nx + ny * ny + nz * nz)
    n = np.stack([nx / ln, ny / ln, nz / ln], -1)
    return ((n * 0.5 + 0.5) * 255).astype(np.uint8)


def polar(lat):
    return np.abs(np.sin(lat))[:, None]


# --- class looks: each returns (albedo HxWx3 0..1, height 0..1, clouds or None, night or None) ---------------------

def look_m(p, lat, v):
    h = norm01(fbm(p, v, 9, 1.6), 1, 99)
    sea = 0.5 + 0.03 * (v % 3)
    land = h > sea
    ocean = ramp(smooth(0, sea, h), [(0, hexc('#0a2a5c')), (0.7, hexc('#1b5fa0')), (1, hexc('#4aa0c8'))])
    hl = np.clip((h - sea) / (1 - sea), 0, 1)
    moist = smooth(0.2, 0.42, fbm(p, v + 8, 5, 2.5))[..., None]
    green = ramp(hl, [(0, hexc('#3f8a4a')), (0.35, hexc('#58a05a')), (0.6, hexc('#8a8a5a')), (0.85, hexc('#8d8378')), (1, hexc('#f1f1f1'))])
    desert = ramp(hl, [(0, hexc('#c4a870')), (0.5, hexc('#a98f66')), (1, hexc('#8d8378'))])
    landc = green * moist + desert * (1 - moist)
    alb = np.where(land[..., None], landc, ocean)
    ice = smooth(0.9, 0.96, polar(lat) + 0.08 * (fbm(p, v + 4, 5, 4) - 0.5))
    alb = alb * (1 - ice[..., None]) + col('#eef4f8') * ice[..., None]
    height = np.where(land, 0.3 + 0.7 * hl, 0.25 * h / sea)
    return alb, height, clouds_field(p, v, 0.55), night_lights(p, (land & (ice < 0.3)).astype(np.float32), v, 0.35)


def look_o(p, lat, v):
    h = norm01(fbm(p, v, 9, 1.8), 1, 99)
    sea = 0.86 + 0.01 * (v % 3)
    land = h > sea
    depth = smooth(0, sea, h)
    ocean = ramp(depth, [(0, hexc('#08234d')), (0.6, hexc('#1a5aa6')), (0.9, hexc('#2f8fc4')), (1, hexc('#6fd0d0'))])
    isl = ramp(np.clip((h - sea) / (1 - sea), 0, 1), [(0, hexc('#d6c590')), (0.4, hexc('#4d9a55')), (1, hexc('#355f3c'))])
    alb = np.where(land[..., None], isl, ocean)
    ice = smooth(0.88, 0.96, polar(lat))
    alb = alb * (1 - ice[..., None]) + col('#e8f1f6') * ice[..., None]
    height = np.where(land, 0.4 + 0.6 * (h - sea) / (1 - sea), 0.2 * depth)
    return alb, height, clouds_field(p, v + 1, 0.7), night_lights(p, land.astype(np.float32), v, 0.15)


def look_p(p, lat, v):
    h = norm01(fbm(p, v, 9, 2.0), 1, 99)
    ridge = fbm(p, v + 2, 7, 3.0, ridged=True)
    cracks = smooth(0.82, 0.95, ridge)
    rock = ramp(h, [(0, hexc('#4a4f58')), (1, hexc('#79808c'))])
    ice = ramp(norm01(fbm(p, v + 6, 6, 2.5)), [(0, hexc('#b8d6ea')), (0.5, hexc('#dcebf5')), (1, hexc('#f8fcff'))])
    cover = smooth(0.18, 0.35, h + 0.3 * polar(lat))
    alb = rock * (1 - cover[..., None]) + ice * cover[..., None]
    alb = alb * (1 - 0.35 * cracks[..., None]) + col('#5fa0c8') * 0.2 * cracks[..., None]
    height = 0.6 * h + 0.4 * (1 - cracks)
    nl = night_lights(p, ((polar(lat) < 0.6) * (h < 0.4)).astype(np.float32), v, 0.1, (0.9, 0.95, 1.0))
    return alb, height, clouds_field(p, v + 3, 0.35, 0.8), nl


def look_l(p, lat, v):
    h = norm01(fbm(p, v, 9, 1.7), 1, 99)
    water = h < 0.3
    j = norm01(fbm(p, v + 2, 8, 6.0))
    jungle = ramp(j, [(0, hexc('#1f4a22')), (0.5, hexc('#3d7a2f')), (1, hexc('#7fa35a'))])
    jungle = jungle * (0.8 + 0.3 * norm01(fbm(p, v + 3, 5, 4.0))[..., None])
    wat = ramp(h / 0.3, [(0, hexc('#2f5e48')), (1, hexc('#4f8f6a'))])
    alb = np.where(water[..., None], wat, jungle)
    ice = smooth(0.9, 0.97, polar(lat))
    alb = alb * (1 - ice[..., None]) + col('#e0ece0') * ice[..., None]
    height = np.where(water, 0.1 * h / 0.3, 0.3 + 0.5 * j + 0.2 * (h - 0.3))
    return alb, height, clouds_field(p, v + 4, 0.72), night_lights(p, (~water).astype(np.float32), v, 0.25)


def look_h(p, lat, v):
    dune = norm01(fbm(p * np.array([1, 3.0, 1], np.float32), v, 8, 3.0))
    base = ramp(dune, [(0, hexc('#a8723c')), (0.4, hexc('#cf9c58')), (0.75, hexc('#e0b878')), (1, hexc('#ecd3a0'))])
    h = norm01(fbm(p, v + 5, 8, 1.8), 1, 99)
    lake = smooth(0.18, 0.12, h)[..., None]
    rock = ramp(norm01(fbm(p, v + 7, 6, 5.0)), [(0, hexc('#5a3f2c')), (1, hexc('#8a6544'))])
    rockm = smooth(0.74, 0.86, h)[..., None] * 0.7
    alb = base * (1 - rockm) + rock * rockm
    alb = alb * (1 - lake * 0.9) + col('#e8dcc2') * lake * 0.9
    ice = smooth(0.93, 0.98, polar(lat))
    alb = alb * (1 - ice[..., None]) + col('#f0eadc') * ice[..., None]
    nl = night_lights(p, ((1 - lake[..., 0]) * (polar(lat) < 0.8)).astype(np.float32), v, 0.12)
    return alb, 0.5 * dune + 0.5 * h, clouds_field(p, v + 6, 0.22, 0.7), nl


def look_k(p, lat, v):
    hgt = norm01(fbm(p, v, 8, 2.0)) * 0.4
    crat = 0
    for i, (fr, amp) in enumerate([(5, 1.0), (11, 0.7), (24, 0.5), (50, 0.3)]):
        d = worley(p, fr, v * 10 + i)
        bowl = -smooth(0.45, 0.0, d) * 0.9 + smooth(0.40, 0.48, d) * smooth(0.6, 0.48, d) * 0.45
        crat = crat + bowl * amp * (0.5 + 0.5 * vnoise(p, 3.0, v + i))
    height = norm01(hgt + crat * 0.45)
    mare = smooth(0.5, 0.65, fbm(p, v + 3, 4, 1.5))[..., None]
    g = ramp(height, [(0, hexc('#585250')), (0.5, hexc('#8a7f78')), (1, hexc('#b4aaa2'))])
    alb = g * (1 - 0.4 * mare) + col('#4f4a48') * 0.4 * mare
    return alb, height, None, None


def look_y(p, lat, v):
    h = norm01(fbm(p, v, 9, 1.8), 1, 99)
    rock = ramp(h, [(0, hexc('#1a0f0d')), (0.5, hexc('#3e2018')), (1, hexc('#7a3a26'))])
    lava = smooth(0.80, 0.93, fbm(p, v + 3, 7, 3.0, ridged=True))
    glow = np.clip(lava * 0.8 + smooth(0.25, 0.1, h), 0, 1)[..., None]
    alb = rock * (1 - glow * 0.6) + col('#d84a20') * glow * 0.6
    nl = (glow * np.array([1.0, 0.35, 0.08], np.float32)).astype(np.float32)
    return alb, 0.7 * h + 0.3 * (1 - lava), clouds_field(p, v + 6, 0.8, 1.0), nl


GAS_SETS = [
    [(0, '#8a6a42'), (0.2, '#c9a064'), (0.4, '#e6cf9c'), (0.6, '#b8895a'), (0.8, '#dcbc88'), (1, '#9a7448')],  # tan
    [(0, '#2d5a66'), (0.2, '#4d9a98'), (0.4, '#a8d4c6'), (0.6, '#3f8a8f'), (0.8, '#8cc4b8'), (1, '#355f70')],  # blue-green
    [(0, '#6a2c1c'), (0.2, '#a8502c'), (0.4, '#dd9a64'), (0.6, '#8f3e24'), (0.8, '#c2704a'), (1, '#4d2418')],  # rust
]


def look_gas(p, lat, v):
    vi = ((v - 100) % 1000) // 17  # variant number 1..6 (the seed carries the class and variant)
    stops = [(a, hexc(b)) for a, b in GAS_SETS[(vi - 1) % 3]]
    squash = np.array([1, 0.2, 1], np.float32)
    warp = fbm(p * squash, v, 6, 3.0)
    y = p[..., 1] + (warp - 0.5) * 0.35 + 0.04 * (fbm(p, v + 2, 4, 8.0) - 0.5)
    bands = 0.5 + 0.5 * np.sin(y * (9 + 2 * ((vi - 1) // 3) + vi % 3) * np.pi + 2.0 * np.sin(y * 31))
    tex = norm01(fbm(p * np.array([1, 0.15, 1], np.float32), v + 3, 7, 6.0))
    t = np.clip(bands * 0.7 + (tex - 0.5) * 0.5 + 0.15, 0, 1)
    alb = ramp(t, stops)
    s = np.array([math.cos(0.4) * math.cos(1.2 + vi), math.sin(-0.35 + 0.12 * vi), math.cos(0.4) * math.sin(1.2 + vi)], np.float32)
    d = np.arccos(np.clip((p * s).sum(-1), -1, 1))
    spot = smooth(0.22, 0.05, d * (1 + 0.5 * (fbm(p, v + 5, 3, 6.0) - 0.5)))[..., None]
    alb = alb * (1 - spot * 0.8) + ramp(np.full(spot.shape[:2], 0.55), stops) * np.array([1.08, 0.78, 0.7], np.float32) * spot * 0.8
    return np.clip(alb, 0, 1), t, None, None


LOOKS = {'m': look_m, 'o': look_o, 'p': look_p, 'l': look_l, 'h': look_h, 'k': look_k, 'y': look_y, 'gasgiant': look_gas}
NORMAL_STRENGTH = {'m': 5, 'o': 3, 'p': 6, 'l': 5, 'h': 7, 'k': 9, 'y': 8}


def to8(a):
    return (np.clip(a, 0, 1) * 255 + 0.5).astype(np.uint8)


def up(img8, size):
    return np.asarray(Image.fromarray(img8).resize(size, Image.LANCZOS))


def make_variant(args):
    cls, v = args
    dd = os.path.join(OUT, cls, 'desktop')
    wd = os.path.join(OUT, cls, 'web')
    os.makedirs(dd, exist_ok=True)
    os.makedirs(wd, exist_ok=True)
    p, lat = sphere_dirs(W, H)
    seed = 100 + 17 * v + 1000 * CLASSES.index(cls)
    alb, height, cl, nl = LOOKS[cls](p, lat, seed)
    p4, _ = sphere_dirs(W * 2, H * 2)
    grain = fbm(p4, seed + 90, 2, 96.0) - 0.5
    alb8 = up(to8(alb), (W * 2, H * 2)).astype(np.float32) / 255
    alb8 = np.clip(alb8 * (1 + 0.16 * grain[..., None]), 0, 1)
    Image.fromarray(to8(alb8)).save(os.path.join(dd, 'v%d_albedo.png' % v), compress_level=3)
    normal = None
    if cls in NORMAL_STRENGTH:
        normal = normal_from_height(height.astype(np.float32), NORMAL_STRENGTH[cls] * 2.5)
        Image.fromarray(normal).save(os.path.join(dd, 'v%d_normal.png' % v))
    if cl is not None:
        Image.fromarray(to8(cl), 'L').save(os.path.join(dd, 'v%d_clouds.png' % v))
    if nl is not None:
        Image.fromarray(to8(nl)).save(os.path.join(dd, 'v%d_night.png' % v))
    if v <= 4:
        small = np.asarray(Image.fromarray(to8(alb8)).resize((1024, 512), Image.LANCZOS)).astype(np.float32) / 255
        if normal is not None:  # bake relief into the web albedo
            n = normal.astype(np.float32) / 255 * 2 - 1
            shade = np.clip(1.0 + 0.9 * (n[..., 0] * -0.5 + n[..., 1] * 0.5 + 0.7 * (n[..., 2] - 1)), 0.55, 1.35)
            sh = np.asarray(Image.fromarray(to8(shade / 1.35)).resize((1024, 512), Image.LANCZOS)).astype(np.float32) / 255 * 1.35
            small = small * sh[..., None]
        Image.fromarray(to8(small)).save(os.path.join(wd, 'v%d_albedo.webp' % v), quality=82, method=4)
        if cl is not None:
            Image.fromarray(to8(cl), 'L').resize((512, 256), Image.LANCZOS).save(os.path.join(wd, 'v%d_clouds.png' % v))
        if nl is not None:
            Image.fromarray(to8(nl)).resize((512, 256), Image.LANCZOS).save(os.path.join(wd, 'v%d_night.png' % v))
    return cls, v


def icosphere(sub):
    t = (1 + 5 ** 0.5) / 2
    v = [(-1, t, 0), (1, t, 0), (-1, -t, 0), (1, -t, 0), (0, -1, t), (0, 1, t), (0, -1, -t), (0, 1, -t), (t, 0, -1), (t, 0, 1), (-t, 0, -1), (-t, 0, 1)]
    f = [(0, 11, 5), (0, 5, 1), (0, 1, 7), (0, 7, 10), (0, 10, 11), (1, 5, 9), (5, 11, 4), (11, 10, 2), (10, 7, 6), (7, 1, 8),
         (3, 9, 4), (3, 4, 2), (3, 2, 6), (3, 6, 8), (3, 8, 9), (4, 9, 5), (2, 4, 11), (6, 2, 10), (8, 6, 7), (9, 8, 1)]
    v = [tuple(np.array(a) / np.linalg.norm(a)) for a in v]
    for _ in range(sub):
        cache, nf = {}, []

        def mid(a, b):
            k = (min(a, b), max(a, b))
            if k not in cache:
                m = (np.array(v[a]) + np.array(v[b])) / 2
                v.append(tuple(m / np.linalg.norm(m)))
                cache[k] = len(v) - 1
            return cache[k]
        for a, b, c in f:
            ab, bc, ca = mid(a, b), mid(b, c), mid(c, a)
            nf += [(a, ab, ca), (b, bc, ab), (c, ca, bc), (ab, bc, ca)]
        f = nf
    return np.array(v, np.float32), f


def make_asteroids():
    d = os.path.join(OUT, 'asteroids')
    os.makedirs(os.path.join(d, 'desktop'), exist_ok=True)
    os.makedirs(os.path.join(d, 'web'), exist_ok=True)
    rng = np.random.default_rng(777)
    p, lat = sphere_dirs(1024, 512)
    cr = 0
    for i, fr in enumerate((4, 9, 20)):
        dd = worley(p, fr, 910 + i)
        cr = cr - smooth(0.4, 0.0, dd) * 0.5 + smooth(0.38, 0.46, dd) * smooth(0.6, 0.46, dd) * 0.25
    hgt = norm01(fbm(p, 901, 8, 3.0) + 0.5 * cr)
    alb = ramp(hgt, [(0, hexc('#4a4a50')), (0.5, hexc('#707078')), (1, hexc('#9a9aa0'))]) * (0.85 + 0.3 * fbm(p, 902, 5, 12)[..., None])
    Image.fromarray(to8(alb)).save(os.path.join(d, 'desktop', 'rock_albedo.png'))
    Image.fromarray(normal_from_height(hgt.astype(np.float32), 40)).save(os.path.join(d, 'desktop', 'rock_normal.png'))
    Image.fromarray(to8(alb)).resize((512, 256), Image.LANCZOS).save(os.path.join(d, 'web', 'rock_albedo.webp'), quality=80)
    for i in range(6):
        for tag, sub in (('desktop', 3), ('web', 2)):
            v, faces = icosphere(sub)
            sc = np.array([1.0, rng.uniform(0.6, 0.95), rng.uniform(0.7, 1.0)])
            n1 = fbm(v, 500 + i, 5, 1.5) - 0.5
            n2 = worley(v, 2.5 + i * 0.4, 520 + i)
            r = 1 + 0.55 * n1 - 0.25 * smooth(0.5, 0.0, n2)
            vv = v * r[:, None] * sc
            vv /= np.abs(vv).max()
            u = 0.5 + np.arctan2(v[:, 2], v[:, 0]) / (2 * np.pi)
            w = 0.5 - np.arcsin(np.clip(v[:, 1], -1, 1)) / np.pi
            with open(os.path.join(d, tag, 'rock_%d.obj' % (i + 1)), 'w') as fh:
                fh.write('# L7 procedural asteroid %d (original work)\n' % (i + 1))
                for a in vv:
                    fh.write('v %.5f %.5f %.5f\n' % tuple(a))
                for a, b in zip(u, w):
                    fh.write('vt %.5f %.5f\n' % (a, b))
                for t in faces:
                    fh.write('f ' + ' '.join('%d/%d' % (k + 1, k + 1) for k in t) + '\n')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--classes', default=','.join(CLASSES))
    ap.add_argument('--variants', type=int, default=6)
    ap.add_argument('--workers', type=int, default=3)
    a = ap.parse_args()
    cl = a.classes.split(',')
    jobs = [(c, v) for c in cl if c in LOOKS for v in range(1, a.variants + 1)]
    if os.name == 'nt':
        import ctypes
        ctypes.windll.kernel32.SetPriorityClass(ctypes.windll.kernel32.GetCurrentProcess(), C.BELOW_NORMAL)
    if 'asteroids' in cl:
        make_asteroids()
        print('asteroids: 6 rock meshes + rock maps', flush=True)
    with ProcessPoolExecutor(a.workers) as ex:
        for c, v in ex.map(make_variant, jobs):
            print('planet.%s variant %d done' % (c, v), flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
