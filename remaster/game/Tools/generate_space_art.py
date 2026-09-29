"""Generates the battle scene's original environment art (no third-party input).

    python generate_space_art.py [--out ../ArtSource/Generated/Environment] [--size 8192]

Writes:
  T_SpaceNebula.png  equirectangular sky: layered nebula (domain-warped fBm sampled on the unit sphere, so there is no
                     seam and no pole pinch), a galactic band, and three star populations with colour temperature.
  T_GasGiant.png     equirectangular banded gas giant with turbulent belts and a storm.
  T_RockyMoon.png    equirectangular rocky moon (cratered noise), used as a second body.

Everything is deterministic (fixed seeds). ue_build_environment.py imports these into /Game/Generated/Environment.
"""
import argparse
import os

import numpy as np
from PIL import Image
from scipy import ndimage

RNG_SEED = 1701


# ---------------------------------------------------------------- 3D value noise (vectorised, integer-hashed)
def _hash(ix, iy, iz, seed):
    h = (ix * 73856093) ^ (iy * 19349663) ^ (iz * 83492791) ^ (seed * 2654435761)
    h = (h ^ (h >> 13)) * 1274126177
    h = h ^ (h >> 16)
    return (h & 0xFFFFFF).astype(np.float32) / float(0xFFFFFF)


def value_noise3(p, seed):
    i = np.floor(p).astype(np.int64)
    f = (p - i).astype(np.float32)
    u = f * f * f * (f * (f * 6 - 15) + 10)  # quintic fade
    x0, y0, z0 = i[..., 0], i[..., 1], i[..., 2]
    x1, y1, z1 = x0 + 1, y0 + 1, z0 + 1
    c000 = _hash(x0, y0, z0, seed); c100 = _hash(x1, y0, z0, seed)
    c010 = _hash(x0, y1, z0, seed); c110 = _hash(x1, y1, z0, seed)
    c001 = _hash(x0, y0, z1, seed); c101 = _hash(x1, y0, z1, seed)
    c011 = _hash(x0, y1, z1, seed); c111 = _hash(x1, y1, z1, seed)
    ux, uy, uz = u[..., 0], u[..., 1], u[..., 2]
    x00 = c000 + (c100 - c000) * ux; x10 = c010 + (c110 - c010) * ux
    x01 = c001 + (c101 - c001) * ux; x11 = c011 + (c111 - c011) * ux
    y0v = x00 + (x10 - x00) * uy; y1v = x01 + (x11 - x01) * uy
    return y0v + (y1v - y0v) * uz


def fbm3(p, seed, octaves=6, lacunarity=2.03, gain=0.5):
    total = np.zeros(p.shape[:-1], np.float32)
    amp, norm, q = 1.0, 0.0, p.astype(np.float32)
    for o in range(octaves):
        total += amp * value_noise3(q, seed + o * 101)
        norm += amp
        amp *= gain
        q = q * lacunarity + np.float32(17.17)
    return total / norm


def ridged3(p, seed, octaves=5):
    total = np.zeros(p.shape[:-1], np.float32)
    amp, norm, q = 1.0, 0.0, p.astype(np.float32)
    for o in range(octaves):
        n = 1.0 - np.abs(value_noise3(q, seed + o * 57) * 2.0 - 1.0)
        total += amp * n * n
        norm += amp
        amp *= 0.5
        q = q * 2.1 + np.float32(3.3)
    return total / norm


def sphere_dirs(width, height):
    lon = (np.arange(width, dtype=np.float32) + 0.5) / width * 2 * np.pi - np.pi
    lat = np.pi / 2 - (np.arange(height, dtype=np.float32) + 0.5) / height * np.pi
    lon, lat = np.meshgrid(lon, lat)
    return np.stack([np.cos(lat) * np.cos(lon), np.cos(lat) * np.sin(lon), np.sin(lat)], axis=-1)


def smoothstep(a, b, x):
    t = np.clip((x - a) / (b - a), 0, 1)
    return t * t * (3 - 2 * t)


def ramp(t, stops):
    """Piecewise-linear colour ramp; stops = [(pos, (r,g,b)), ...]."""
    t = np.clip(t, 0, 1)
    out = np.zeros(t.shape + (3,), np.float32)
    for (p0, c0), (p1, c1) in zip(stops[:-1], stops[1:]):
        m = (t >= p0) & (t <= p1)
        k = ((t[m] - p0) / max(p1 - p0, 1e-6))[:, None]
        out[m] = np.array(c0, np.float32) * (1 - k) + np.array(c1, np.float32) * k
    return out


def to_srgb8(linear):
    x = np.clip(linear, 0, None)
    x = x / (1.0 + x * 0.18)  # gentle shoulder so bright cores keep colour
    srgb = np.where(x <= 0.0031308, 12.92 * x, 1.055 * np.power(np.clip(x, 1e-8, None), 1 / 2.4) - 0.055)
    return (np.clip(srgb, 0, 1) * 255 + 0.5).astype(np.uint8)


# ---------------------------------------------------------------- sky
def nebula_layer(d, seed, scale, warp, stops, density_bias):
    q = d * scale
    w = np.stack([fbm3(q + 11.0, seed + 1, 4), fbm3(q + 23.0, seed + 2, 4), fbm3(q + 37.0, seed + 3, 4)], -1)
    n = fbm3(q + (w - 0.5) * warp, seed, 7)
    detail = ridged3(q * 2.3 + w, seed + 9, 5)
    density = smoothstep(density_bias, density_bias + 0.32, n) * (0.55 + 0.75 * detail)
    colour = ramp(np.clip(n * 1.25 - 0.1 + (detail - 0.5) * 0.3, 0, 1), stops)
    return colour * density[..., None], density


def make_sky(width, height, rng):
    low_w, low_h = width // 2, height // 2
    d = sphere_dirs(low_w, low_h)

    # Galactic band: a great circle tilted against the equator.
    axis = np.array([0.25, -0.35, 0.90], np.float32); axis /= np.linalg.norm(axis)
    band = np.exp(-np.square((d @ axis) / 0.20))
    band_noise = fbm3(d * 3.0, 777, 6)

    violet = [(0.0, (0.02, 0.01, 0.05)), (0.35, (0.20, 0.05, 0.35)), (0.6, (0.65, 0.18, 0.55)), (0.85, (1.4, 0.55, 0.65)), (1.0, (2.2, 1.4, 1.2))]
    teal = [(0.0, (0.00, 0.02, 0.03)), (0.4, (0.03, 0.22, 0.30)), (0.7, (0.10, 0.60, 0.70)), (1.0, (0.9, 1.6, 1.5))]
    ember = [(0.0, (0.03, 0.01, 0.0)), (0.45, (0.35, 0.10, 0.03)), (0.75, (1.1, 0.45, 0.12)), (1.0, (2.0, 1.2, 0.6))]

    c1, d1 = nebula_layer(d, 1001, 1.35, 1.8, violet, 0.46)
    c2, d2 = nebula_layer(d, 2002, 2.10, 2.4, teal, 0.52)
    c3, d3 = nebula_layer(d, 3003, 1.70, 1.5, ember, 0.54)

    # Most of the sky is dark: nebulae gather in a few large regions (low-frequency masks), the brightest near the band.
    region_a = smoothstep(0.44, 0.74, fbm3(d * 0.85 + 3.0, 8008, 4)) * (0.55 + 0.45 * band)
    region_b = smoothstep(0.48, 0.76, fbm3(d * 1.10 + 9.0, 8009, 4))
    region_c = smoothstep(0.52, 0.78, fbm3(d * 1.30 + 21.0, 8010, 4))
    c1 *= region_a[..., None]; c2 *= region_b[..., None]; c3 *= (region_c * region_a)[..., None]

    # Dust lanes absorb what is behind them.
    dust = smoothstep(0.52, 0.72, fbm3(d * 4.2 + 5.0, 4004, 6)) * smoothstep(0.35, 0.8, band + d1 * 0.6)
    sky = (c1 * 1.9 + c2 * 1.4 + c3 * 1.3) * (1.0 - 0.75 * dust[..., None])
    band_core = np.exp(-np.square((d @ axis) / 0.07))
    band_glow = band * smoothstep(0.35, 0.85, band_noise) * 0.9 + band_core * smoothstep(0.3, 0.9, band_noise) * 0.8
    band_colour = np.array([0.14, 0.12, 0.20], np.float32) + np.array([0.20, 0.13, 0.05], np.float32) * band_core[..., None]
    sky += band_colour * band_glow[..., None] * (1.0 - 0.85 * dust[..., None])
    sky += np.array([0.004, 0.005, 0.010], np.float32)  # deep-space floor

    sky = np.stack([ndimage.zoom(sky[..., c], 2, order=3) for c in range(3)], -1)[:height, :width]
    band_hi = ndimage.zoom(band, 2, order=1)[:height, :width]

    # Stars: counts per latitude row are weighted by cos(lat) so density is uniform on the sphere.
    lat_weight = np.cos(np.pi / 2 - (np.arange(height) + 0.5) / height * np.pi)

    def scatter(count, band_bias):
        rows = rng.choice(height, size=count, p=lat_weight / lat_weight.sum())
        cols = rng.integers(0, width, size=count)
        keep = rng.random(count) < (1 - band_bias) + band_bias * band_hi[rows, cols]
        return rows[keep], cols[keep]

    def star_colour(n):
        t = rng.random(n)  # blackbody-ish: red dwarfs to blue giants
        return ramp(t, [(0.0, (1.0, 0.55, 0.35)), (0.3, (1.0, 0.82, 0.62)), (0.6, (1.0, 0.97, 0.92)), (0.85, (0.75, 0.85, 1.0)), (1.0, (0.55, 0.70, 1.0))])

    stars = np.zeros_like(sky)
    r, c = scatter(int(width * height * 0.0022), 0.55)
    stars[r, c] += star_colour(len(r)) * rng.uniform(0.15, 0.9, len(r))[:, None].astype(np.float32)
    r, c = scatter(int(width * height * 0.00012), 0.35)
    mid = np.zeros_like(sky)
    mid[r, c] += star_colour(len(r)) * rng.uniform(1.5, 4.0, len(r))[:, None].astype(np.float32)
    stars += np.stack([ndimage.gaussian_filter(mid[..., k], 0.9) for k in range(3)], -1) * 2.5
    r, c = scatter(int(width * height * 0.000004), 0.2)
    big = np.zeros_like(sky)
    big[r, c] += star_colour(len(r)) * rng.uniform(30, 80, len(r))[:, None].astype(np.float32)
    halo = np.stack([ndimage.gaussian_filter(big[..., k], 2.2) + 0.15 * ndimage.gaussian_filter(big[..., k], 9.0) for k in range(3)], -1)
    stars += halo

    return to_srgb8(sky * 1.0 + stars)


# ---------------------------------------------------------------- bodies
def make_gas_giant(width, height, rng):
    d = sphere_dirs(width, height)
    lat = d[..., 2]
    turb = fbm3(d * np.array([6.0, 6.0, 1.5], np.float32), 5005, 6)
    swirl = fbm3(d * np.array([14.0, 14.0, 3.0], np.float32) + turb[..., None] * 2.0, 5006, 5)
    bands = np.sin(lat * 23.0 + turb * 3.2 + swirl * 1.1) * 0.5 + 0.5
    belts = smoothstep(0.35, 0.65, np.sin(lat * 9.0 + turb * 1.5) * 0.5 + 0.5)
    t = np.clip(bands * 0.6 + belts * 0.3 + (swirl - 0.5) * 0.4, 0, 1)
    colour = ramp(t, [(0.0, (0.28, 0.14, 0.07)), (0.3, (0.62, 0.36, 0.18)), (0.55, (0.86, 0.70, 0.50)), (0.8, (0.95, 0.88, 0.74)), (1.0, (0.78, 0.52, 0.34))])
    # Great storm
    storm_dir = np.array([np.cos(-0.35) * np.cos(0.8), np.cos(-0.35) * np.sin(0.8), np.sin(-0.35)], np.float32)
    s = np.clip(d @ storm_dir, -1, 1)
    spot = smoothstep(0.985, 0.998, s + (swirl - 0.5) * 0.004)
    colour = colour * (1 - spot[..., None]) + np.array([0.75, 0.28, 0.14], np.float32) * spot[..., None]
    return to_srgb8(colour * 0.9)


def make_moon(width, height, rng):
    d = sphere_dirs(width, height)
    base = fbm3(d * 3.0, 6006, 7)
    craters = ridged3(d * 9.0, 6007, 4)
    t = np.clip(base * 0.8 + craters * 0.35 - 0.1, 0, 1)
    colour = ramp(t, [(0.0, (0.10, 0.10, 0.11)), (0.5, (0.30, 0.29, 0.28)), (1.0, (0.62, 0.60, 0.57))])
    return to_srgb8(colour)


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(here, "..", "ArtSource", "Generated", "Environment"))
    ap.add_argument("--size", type=int, default=8192)
    args = ap.parse_args()
    os.makedirs(args.out, exist_ok=True)
    rng = np.random.default_rng(RNG_SEED)
    w = args.size
    Image.fromarray(make_sky(w, w // 2, rng)).save(os.path.join(args.out, "T_SpaceNebula.png"), optimize=True)
    print("T_SpaceNebula.png")
    Image.fromarray(make_gas_giant(4096, 2048, rng)).save(os.path.join(args.out, "T_GasGiant.png"), optimize=True)
    print("T_GasGiant.png")
    Image.fromarray(make_moon(2048, 1024, rng)).save(os.path.join(args.out, "T_RockyMoon.png"), optimize=True)
    print("T_RockyMoon.png")


if __name__ == "__main__":
    main()
