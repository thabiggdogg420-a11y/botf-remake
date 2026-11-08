"""Fast numpy preview of a planet texture set (a lit sphere at 3 rotations, no Godot): for tuning gen_planets.py.

    python tools/art/planet_preview.py m 1 [more classes...] --out C:/tmp/pv.png

Orthographic sphere, key light from the upper left, albedo x Lambert, ocean/ice sun glint from the rough map, clouds as
a translucent white layer, night lights/lava on the dark side. A proxy only: the judged captures are the Godot turntable.
"""
import math
import os
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import _common as C


def load(cls, v):
    d = os.path.join(C.LOCAL, 'planets', cls, 'desktop')
    g = lambda n: os.path.join(d, 'v%d_%s.png' % (v, n))
    alb = np.asarray(Image.open(g('albedo')).convert('RGB').resize((2048, 1024)), np.float32) / 255
    cl = np.asarray(Image.open(g('clouds')).convert('L').resize((2048, 1024)), np.float32) / 255 if os.path.exists(g('clouds')) else None
    nl = np.asarray(Image.open(g('night')).convert('RGB').resize((2048, 1024)), np.float32) / 255 if os.path.exists(g('night')) else None
    rg = np.asarray(Image.open(g('rough')).convert('L').resize((2048, 1024)), np.float32) / 255 if os.path.exists(g('rough')) else None
    nm = np.asarray(Image.open(g('normal')).convert('RGB').resize((2048, 1024)), np.float32) / 255 * 2 - 1 if os.path.exists(g('normal')) else None
    return alb, cl, nl, rg, nm


def sphere(alb, cl, nl, rg, nm, rot, size=480, phase_deg=40.0, cloud_alpha=0.88, night_always=False):
    s = size
    yy, xx = np.mgrid[0:s, 0:s].astype(np.float32)
    x = (xx - s / 2 + 0.5) / (s / 2 * 0.92)
    y = -(yy - s / 2 + 0.5) / (s / 2 * 0.92)
    r2 = x * x + y * y
    m = r2 < 1
    z = np.sqrt(np.clip(1 - r2, 0, 1))
    # view-space normal; planet rotated about Y by rot
    cr, sr = math.cos(rot), math.sin(rot)
    px = x * cr + z * sr
    pz = -x * sr + z * cr
    lon = (np.arctan2(pz, px)) % (2 * np.pi)
    lat = np.arcsin(np.clip(y, -1, 1))
    u = (lon / (2 * np.pi) * 2048).astype(int) % 2048
    v = np.clip(((0.5 - lat / np.pi) * 1024).astype(int), 0, 1023)
    ph = math.radians(phase_deg)
    L = np.array([-math.sin(ph), 0.35, math.cos(ph)], np.float32)
    L /= np.linalg.norm(L)
    nrm = np.stack([x, y, z], -1)
    ndl = np.clip((nrm * L).sum(-1), 0, 1)
    a = alb[v, u]
    if nm is not None:  # perturb the lighting by the tangent normal (screen-aligned approximation)
        n = nm[v, u]
        ndl = np.clip(((nrm + 0.0 * n) * L).sum(-1) + (n[..., 0] * L[0] + n[..., 1] * L[1]) * 0.9 * (ndl > 0), 0, 1) if True else ndl
    a_lin = a ** 2.2
    col = a_lin * ndl[..., None] * 3.0
    if rg is not None:
        rough = rg[v, u]
        Hh = L + np.array([0, 0, 1], np.float32)
        Hh /= np.linalg.norm(Hh)
        nh = np.clip((nrm * Hh).sum(-1), 0, 1)
        spec = (nh ** (2.0 / np.maximum(rough, 0.05) ** 2 * 0.6)) * (1 - rough) ** 1.2 * 1.5 * ndl
        col = col + spec[..., None]
    if cl is not None:
        c = cl[v, u] * cloud_alpha
        col = col * (1 - c[..., None]) + (c * ndl * 3.0)[..., None] * np.array([1, 1, 1], np.float32)
    if nl is not None:
        n_ = nl[v, u] ** 2.2
        k = 1.0 if night_always else np.clip(1 - ndl * 6, 0, 1)[..., None]
        col = col + n_ * k * 2.0
    # atmosphere rim
    fres = (1 - z) ** 3
    col = col + fres[..., None] * np.array([0.25, 0.45, 0.9], np.float32) * (ndl[..., None] * 0.9 + 0.05) * 0.8
    out = col / (1 + col)  # soft tonemap
    out = np.clip(out ** (1 / 2.2), 0, 1)
    out[~m] = 0
    return (out * 255).astype(np.uint8)


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    outp = sys.argv[sys.argv.index('--out') + 1] if '--out' in sys.argv else 'preview.png'
    if '--out' in sys.argv:
        args.remove(outp)
    cls_list = args[0::2] if len(args) > 1 and args[1].isdigit() else args
    vs = [int(a) for a in args[1::2]] if len(args) > 1 and args[1].isdigit() else [1] * len(cls_list)
    rows = []
    for cls, v in zip(cls_list, vs):
        alb, cl, nl, rg, nm = load(cls, v)
        night_always = cls == 'y'
        tiles = [sphere(alb, cl, nl, rg, nm, rot, night_always=night_always) for rot in (0.0, 2.1, 4.2)]
        flat = np.asarray(Image.fromarray((alb * 255).astype(np.uint8)).resize((960, 480)))
        rows.append(np.concatenate(tiles + [flat], 1))
    Image.fromarray(np.concatenate(rows, 0)).save(outp)
    print(outp)


if __name__ == '__main__':
    main()
