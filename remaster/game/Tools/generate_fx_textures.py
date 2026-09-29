"""Generates the battle effects' noise texture (original art, no third-party input).

    python generate_fx_textures.py [--out ../ArtSource/Generated/VFX] [--size 1024]

T_FxNoise.png  seamless RGBA: R billowing fBm (fireball body), G ridged fBm (flame filaments), B fine grain (sparks),
               A cellular (debris glow breakup). ue_build_vfx.py imports it and builds the effect materials on it.
Deterministic (fixed seed).
"""
import argparse
import os

import numpy as np
from PIL import Image


def lattice(n, k, rng):
    grid = rng.random((k, k)).astype(np.float32)
    t = np.arange(n, dtype=np.float32) * k / n
    i0 = np.floor(t).astype(int) % k
    i1 = (i0 + 1) % k
    f = t - np.floor(t)
    f = f * f * (3 - 2 * f)
    rows = grid[i0][:, i0] * (1 - f)[None, :] + grid[i0][:, i1] * f[None, :]
    rows1 = grid[i1][:, i0] * (1 - f)[None, :] + grid[i1][:, i1] * f[None, :]
    return rows * (1 - f)[:, None] + rows1 * f[:, None]


def fbm(n, rng, base, octaves, ridged=False):
    out = np.zeros((n, n), np.float32)
    amp, total = 1.0, 0.0
    for o in range(octaves):
        v = lattice(n, base * 2 ** o, rng)
        if ridged:
            v = 1.0 - np.abs(v * 2 - 1)
            v = v * v
        out += amp * v
        total += amp
        amp *= 0.5
    out /= total
    return (out - out.min()) / max(1e-6, out.max() - out.min())


def cellular(n, rng, cells):
    pts = rng.random((cells, 2)) * n
    yy, xx = np.mgrid[0:n, 0:n].astype(np.float32)
    best = np.full((n, n), 1e9, np.float32)
    for px, py in pts:
        dx = np.abs(xx - px)
        dy = np.abs(yy - py)
        dx = np.minimum(dx, n - dx)
        dy = np.minimum(dy, n - dy)
        best = np.minimum(best, np.sqrt(dx * dx + dy * dy))
    best /= best.max()
    return 1.0 - best


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(here, "..", "ArtSource", "Generated", "VFX"))
    ap.add_argument("--size", type=int, default=1024)
    a = ap.parse_args()
    n = a.size
    rng = np.random.default_rng(2375)
    r = fbm(n, rng, 4, 6)
    g = fbm(n, rng, 6, 5, ridged=True)
    b = fbm(n, rng, 32, 3)
    c = cellular(n, rng, 48)
    rgba = np.stack([r, g, b, c], -1)
    os.makedirs(a.out, exist_ok=True)
    Image.fromarray((rgba * 255 + 0.5).astype(np.uint8), "RGBA").save(os.path.join(a.out, "T_FxNoise.png"))
    print("wrote", os.path.abspath(a.out))


if __name__ == "__main__":
    main()
