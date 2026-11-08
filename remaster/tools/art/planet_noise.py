"""Noise, crater and fluid helpers for gen_planets.py (original work, numpy + scipy). Everything is sampled on the unit
sphere, so equirectangular maps are seamless in longitude and have no pole pinch.

- gnoise / fbm / ridged: improved Perlin gradient noise (not value noise: value noise gives the blotchy, blobby look).
- craters: physically-flavoured crater populations (power-law size-frequency N(>D) ~ D^-b, simple bowls below the
  transition diameter, flat-floored complex craters with central peaks above it, rims, ejecta blankets and fresh-crater
  ray systems), stamped into a height map and a brightness map.
- advect: semi-Lagrangian zonal-jet advection with shear-driven eddies, used for the gas-giant cloud decks.
"""
import numpy as np
from scipy import ndimage

_rng = np.random.default_rng(2718)
_PERM = _rng.permutation(256).astype(np.int32)
_G = np.array([(1, 1, 0), (-1, 1, 0), (1, -1, 0), (-1, -1, 0), (1, 0, 1), (-1, 0, 1), (1, 0, -1), (-1, 0, -1),
               (0, 1, 1), (0, -1, 1), (0, 1, -1), (0, -1, -1), (1, 1, 0), (-1, 1, 0), (0, -1, 1), (0, -1, -1)], np.float32)
_GX, _GY, _GZ = _G[:, 0].copy(), _G[:, 1].copy(), _G[:, 2].copy()


def _off(seed):
    r = np.random.default_rng(int(seed) & 0xFFFFFFFF)
    return r.uniform(10, 240, 3).astype(np.float32)


def _fade(t):
    return t * t * t * (t * (t * 6 - 15) + 10)


def gnoise(p, freq, seed):
    """Gradient noise in about [-0.9, 0.9] for points p (..., 3)."""
    q = p * np.float32(freq) + _off(seed)
    f0 = np.floor(q)
    f = q - f0
    i = f0.astype(np.int32)
    u = _fade(f)
    fx, fy, fz = f[..., 0], f[..., 1], f[..., 2]
    ux, uy, uz = u[..., 0], u[..., 1], u[..., 2]
    ix, iy, iz = i[..., 0] & 255, i[..., 1] & 255, i[..., 2] & 255

    def corner(dx, dy, dz):
        h = _PERM[(_PERM[(_PERM[(iz + dz) & 255] + iy + dy) & 255] + ix + dx) & 255] & 15
        return _GX[h] * (fx - dx) + _GY[h] * (fy - dy) + _GZ[h] * (fz - dz)
    x00 = corner(0, 0, 0) * (1 - ux) + corner(1, 0, 0) * ux
    x10 = corner(0, 1, 0) * (1 - ux) + corner(1, 1, 0) * ux
    x01 = corner(0, 0, 1) * (1 - ux) + corner(1, 0, 1) * ux
    x11 = corner(0, 1, 1) * (1 - ux) + corner(1, 1, 1) * ux
    y0 = x00 * (1 - uy) + x10 * uy
    y1 = x01 * (1 - uy) + x11 * uy
    return (y0 * (1 - uz) + y1 * uz).astype(np.float32)


def fbm(p, seed, octaves=8, freq=2.0, gain=0.5, lac=2.0, ridged=False, billow=False):
    """Fractal sum, normalised to about 0..1 (mean 0.5)."""
    tot = np.zeros(p.shape[:-1], np.float32)
    amp, norm = 1.0, 0.0
    for o in range(octaves):
        n = gnoise(p, freq, seed * 31 + o * 7 + 1)
        if ridged:
            n = 1.0 - np.abs(n) * 1.6
            n = n * n
        elif billow:
            n = np.abs(n) * 1.6
        else:
            n = n * 0.75 + 0.5
        tot += n * amp
        norm += amp
        amp *= gain
        freq *= lac
    return tot / norm


def norm01(a, lo=1, hi=99):
    l, h = np.percentile(a[::3, ::3] if a.ndim == 2 else a, [lo, hi])
    return np.clip((a - l) / max(h - l, 1e-6), 0, 1)


def smooth(a, b, x):
    t = np.clip((x - a) / (b - a), 0, 1)
    return t * t * (3 - 2 * t)


def ramp(t, stops):
    xs = [s[0] for s in stops]
    return np.stack([np.interp(t, xs, [s[1][c] for s in stops]) for c in range(3)], -1).astype(np.float32)


def hexc(s):
    s = s.lstrip('#')
    return tuple(int(s[i:i + 2], 16) / 255 for i in (0, 2, 4))


def srgb_stops(stops):
    """Colour stops given in sRGB hex -> the same tuples (kept in sRGB: the maps are sRGB textures)."""
    return [(a, hexc(b)) for a, b in stops]


def sphere_dirs(w, h):
    lon = (np.arange(w, dtype=np.float32) + 0.5) / w * 2 * np.pi
    lat = (0.5 - (np.arange(h, dtype=np.float32) + 0.5) / h) * np.pi
    cl, sl = np.cos(lat)[:, None], np.sin(lat)[:, None]
    x = cl * np.cos(lon)[None, :]
    z = cl * np.sin(lon)[None, :]
    y = np.broadcast_to(sl, x.shape)
    return np.stack([x, y, z], -1).astype(np.float32), lat


def domain_warp(p, seed, amount=0.35, freq=1.6, octaves=4):
    w = np.stack([fbm(p, seed + k * 13, octaves, freq) - 0.5 for k in range(3)], -1)
    q = p + w * amount
    return q / np.maximum(np.linalg.norm(q, axis=-1, keepdims=True), 1e-6)


def normal_from_height(h, px_per_rad, exaggeration=1.0):
    """Tangent-space normal map (uint8) from a height map in planet-radius units (1 = one radius). px_per_rad is the
    map's pixels per radian at the equator."""
    s = px_per_rad * exaggeration
    dx = (np.roll(h, -1, 1) - np.roll(h, 1, 1)) * 0.5 * s
    dy = (np.roll(h, -1, 0) - np.roll(h, 1, 0)) * 0.5 * s
    nx, ny, nz = -dx, dy, np.ones_like(h)
    ln = np.sqrt(nx * nx + ny * ny + nz * nz)
    n = np.stack([nx / ln, ny / ln, nz / ln], -1)
    return ((n * 0.5 + 0.5) * 255 + 0.5).astype(np.uint8)


def shade_from_height(h, px_per_rad, exaggeration=1.0, light=(-0.55, 0.55, 0.63)):
    """Hill-shade (about 0.5..1.5, 1 = flat) used to bake relief into the web albedo."""
    s = px_per_rad * exaggeration
    dx = (np.roll(h, -1, 1) - np.roll(h, 1, 1)) * 0.5 * s
    dy = (np.roll(h, -1, 0) - np.roll(h, 1, 0)) * 0.5 * s
    nx, ny, nz = -dx, dy, np.ones_like(h)
    ln = np.sqrt(nx * nx + ny * ny + nz * nz)
    l = (nx * light[0] + ny * light[1] + nz * light[2]) / ln
    return np.clip(l / light[2], 0.35, 1.6)


# --- craters ------------------------------------------------------------------------------------------------------

def stamp_craters(p, lat, height, bright, seed, n_total, rmin, rmax, slope=2.0, depth_k=0.22, complex_r=0.045,
                  fresh_rays=0, region_weight=None, bowl_only=False):
    """Stamp craters onto height (planet radii; negative = down) and bright (multiplicative albedo field, 1 = no change).

    n_total craters, radius (radians of arc) drawn from a power law with cumulative slope `slope` between rmin and rmax,
    older craters degraded (shallower, lower rim). complex_r: radius where craters switch from bowls to flat-floored
    terraced ones with a central peak. fresh_rays: how many of the largest-but-one craters get bright ray systems.
    region_weight: optional H x W field 0..1 that thins craters out (e.g. younger maria).
    """
    rng = np.random.default_rng(seed)
    H, W = height.shape
    # inverse-CDF power law:  N(>r) ~ r^-slope
    u = rng.random(n_total)
    a = rmin ** -slope
    b = rmax ** -slope
    radii = (a - u * (a - b)) ** (-1.0 / slope)
    order = np.argsort(-radii)  # big first: small craters overprint big ones, like real superposition
    radii = radii[order]
    # uniform on the sphere
    cz = rng.uniform(-1, 1, n_total)[order]
    clon = rng.uniform(0, 2 * np.pi, n_total)[order]
    clat = np.arcsin(cz)
    age = rng.random(n_total)[order]
    ray_ids = set(np.argsort(-radii)[8:8 + fresh_rays].tolist()) if fresh_rays else set()
    pix_per_rad_lat = H / np.pi
    for k in range(n_total):
        r = float(radii[k])
        la, lo = float(clat[k]), float(clon[k])
        if region_weight is not None:
            rr = int(np.clip((0.5 - la / np.pi) * H, 0, H - 1))
            cc = int(np.clip(lo / (2 * np.pi) * W, 0, W - 1))
            if rng.random() > region_weight[rr, cc]:
                continue
        reach = r * (6.0 if k in ray_ids else 2.6)
        r0 = int(max(0, np.floor((0.5 - (la + reach) / np.pi) * H)))
        r1 = int(min(H, np.ceil((0.5 - (la - reach) / np.pi) * H) + 1))
        if r1 - r0 < 1:
            continue
        cl = max(np.cos(la), 1e-3)
        if abs(la) + reach > 1.45 or reach / cl > np.pi:
            cols = np.arange(W)
        else:
            half = int(np.ceil(reach / cl / (2 * np.pi) * W)) + 1
            c0 = int(lo / (2 * np.pi) * W)
            cols = (np.arange(c0 - half, c0 + half + 1)) % W
        P = p[r0:r1][:, cols]
        c = np.array([np.cos(la) * np.cos(lo), np.sin(la), np.cos(la) * np.sin(lo)], np.float32)
        dot = np.clip((P * c).sum(-1), -1, 1)
        ang = np.arccos(dot)
        d = ang / r
        fresh = 1.0 - age[k] ** 0.5 if False else (1.0 - age[k])  # young = large fresh
        simple = r < complex_r
        depth = depth_k * r * (0.35 + 0.65 * fresh) * (1.0 if simple else 0.45)
        rim_h = 0.35 * depth * (0.4 + 0.6 * fresh)
        h = np.zeros_like(d)
        inside = d < 1.0
        if simple:
            bowl = -depth * (1 - d[inside] ** 2) ** 1.0
        else:
            wall = smooth(0.55, 1.0, d[inside])
            floor = -depth * (1 - wall) - depth * 0.15 * wall
            terr = 0.06 * depth * np.sin(d[inside] * 40) * smooth(0.5, 0.95, d[inside]) * (1 - smooth(0.95, 1.0, d[inside]))
            peak = depth * 0.55 * np.exp(-(d[inside] / 0.1) ** 2) * (1 if r > complex_r * 1.2 else 0)
            bowl = floor + terr + peak
        h[inside] = bowl
        rim = rim_h * np.exp(-((d - 1.0) / 0.12) ** 2)
        eject = rim_h * 0.5 * np.clip(d, 1.0, None) ** -3.2 * (d >= 1.0)
        h = h + rim + eject
        # stamp: later (smaller, usually) craters overprint inside their own footprint
        mask = d < 2.6 * (1.0 if k not in ray_ids else 1.0)
        sub = height[r0:r1]
        ex = np.where(inside, 0.0, 1.0)
        cur = sub[:, cols]
        sub[:, cols] = np.where(inside, h + cur * 0.15, cur + h)
        # albedo: fresh craters have bright rims/floors/ejecta, old ones fade; fresh ones get rays
        bsub = bright[r0:r1]
        glow = 1.0 + (0.06 + 0.22 * fresh ** 2) * np.exp(-np.clip(d - 0.9, 0, None) * 1.6) * (d < 2.4)
        glow = np.where(inside, 1.0 + 0.10 * fresh, glow)
        if k in ray_ids:
            # radial rays: angular noise around the crater centre, extending ~6 radii
            bear = np.arctan2(P[..., 2] * c[0] - P[..., 0] * c[2], P[..., 1] - dot * c[1] + 1e-6)
            rays = np.abs(np.sin(bear * 5.5 + 3.0 * np.sin(bear * 2.3 + k) + 7.0 * np.sin(bear * 11.0 + k * 0.7)))
            ray = (rays ** 3) * np.exp(-np.clip(d - 1.0, 0, None) / 2.6) * (d > 0.95) * smooth(0.95, 1.2, d)
            glow = glow + 0.45 * ray
        bsub[:, cols] = bsub[:, cols] * glow
    return height, bright


# --- gas-giant dynamics -------------------------------------------------------------------------------------------

def advect(fields, u_row, eddy_amp, shear_row, steps, dt, seed, psi_freq=3.0):
    """Advect scalar fields (list of H x W arrays) on an equirectangular grid.

    u_row: zonal speed per row (radians of longitude per unit time, already cos-corrected by the caller if wanted).
    Shear-driven eddies: a divergence-free perturbation from a stream-function noise, scaled by |shear_row| (rows).
    Semi-Lagrangian backward tracing, bilinear, periodic in longitude.
    """
    H, W = fields[0].shape
    p, lat = sphere_dirs(W, H)
    psi = fbm(p * np.array([1.0, 2.6, 1.0], np.float32), seed, 6, psi_freq)  # stretched in lat: eddies are wider than tall
    psi = (psi - psi.mean()) * 2.0
    dpsi_dx = (np.roll(psi, -1, 1) - np.roll(psi, 1, 1)) * 0.5
    dpsi_dy = (np.roll(psi, -1, 0) - np.roll(psi, 1, 0)) * 0.5
    amp = (eddy_amp * shear_row)[:, None]
    # velocity in pixels per step: u (zonal) + eddy (v = d psi/dx, u' = -d psi/dy)
    ux = (u_row[:, None] * (W / (2 * np.pi)) * dt) - dpsi_dy * amp * dt * 60
    vy = dpsi_dx * amp * dt * 60
    yy, xx = np.mgrid[0:H, 0:W].astype(np.float32)
    out = [f.copy() for f in fields]
    for s in range(steps):
        cy = np.clip(yy - vy, 0, H - 1)
        cx = xx - ux
        coords = np.stack([cy, cx])
        out = [ndimage.map_coordinates(f, coords, order=1, mode='grid-wrap') for f in out]
        # re-seed: slight evolution of the eddy field so structures are not frozen
        if s % 20 == 19:
            psi = np.roll(psi, 3, 1) * 0.97 + 0.03 * (fbm(p * np.array([1.0, 2.6, 1.0], np.float32), seed + s, 6, psi_freq) - 0.5) * 2
            dpsi_dx = (np.roll(psi, -1, 1) - np.roll(psi, 1, 1)) * 0.5
            dpsi_dy = (np.roll(psi, -1, 0) - np.roll(psi, 1, 0)) * 0.5
            ux = (u_row[:, None] * (W / (2 * np.pi)) * dt) - dpsi_dy * amp * dt * 60
            vy = dpsi_dx * amp * dt * 60
    return out
