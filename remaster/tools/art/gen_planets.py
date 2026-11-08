"""Procedural planet texture sets for every planet class in the asset manifest (original work, numpy + scipy + Pillow).
Art wave 2 (L9) rebuild: photoreal targets are NASA imagery (Blue Marble Earth, the Moon, Mars, Jupiter, Io, Callisto,
Eros) and the planets in the shows' establishing shots, see tools/art/planet_refs.py for the side-by-side sheet.

    python tools/art/gen_planets.py [--classes m,o,...] [--variants 6] [--workers 6] [--work 2048] [--only-variant 1]

Per class and variant (ART-BIBLE section 8, budgets section 12.1), written under LocalAssets/planets/<class>/:
  desktop/v<n>_albedo.png   4096x2048 sRGB, equirectangular, seamless in longitude, no pole pinch
  desktop/v<n>_normal.png   2048x1024 tangent-space relief (physical heights, see planet_noise.normal_from_height)
  desktop/v<n>_rough.png    2048x1024 R8 roughness (oceans and ice low: they carry the sun glint; land high)
  desktop/v<n>_clouds.png   2048x1024 R8 cloud cover (cover 35 to 55 percent with thin edges: the surface stays readable)
  desktop/v<n>_night.png    2048x1024 RGB night-side city lights (colonised classes) or lava emission (planet Y)
  web/v<n>_albedo.webp      1024x512, relief baked in (no normal on web), variants 1-4 only
  web/v<n>_rough.png        512x256 R8; web/v<n>_clouds.png, web/v<n>_night.png  512x256
Heights, biomes and clouds come from fBm and ridged gradient noise on the sphere, biome colour from latitude, altitude
and moisture, craters from a power-law size-frequency distribution, gas-giant decks from a zonal-jet advection with
shear eddies. Asteroids: 6 cratered rock meshes (OBJ with UVs and normals) with per-rock albedo and normal.
Runs at BelowNormal priority. Output is private (LocalAssets is gitignored).
"""
import argparse
import math
import os
import sys
from concurrent.futures import ProcessPoolExecutor

import numpy as np
from PIL import Image, ImageFilter
from scipy import ndimage

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import _common as C
from planet_noise import (fbm, gnoise, norm01, smooth, ramp, hexc, sphere_dirs, domain_warp, normal_from_height,
                          shade_from_height, stamp_craters, advect)

OUT = os.path.join(C.LOCAL, 'planets')
CLASSES = ['m', 'o', 'p', 'l', 'h', 'k', 'y', 'gasgiant', 'asteroids']
WORK_W = 2048  # working resolution (width); height is half


def col(s):
    return np.array(hexc(s), np.float32)


def polar(lat):
    return np.abs(np.sin(lat))[:, None]


def bilinear_table(T, Mo, table):
    """Biome colour from a 4x4 (temperature x moisture) table of sRGB hex colours, bilinear."""
    tab = np.array([[hexc(c) for c in row] for row in table], np.float32)  # [T][Mo][rgb]
    t = np.clip(T, 0, 1) * 3 - 1e-4
    m = np.clip(Mo, 0, 1) * 3 - 1e-4
    ti, mi = np.floor(t).astype(int), np.floor(m).astype(int)
    tf, mf = (t - ti)[..., None], (m - mi)[..., None]
    return (tab[ti, mi] * (1 - tf) * (1 - mf) + tab[ti + 1, mi] * tf * (1 - mf) +
            tab[ti, mi + 1] * (1 - tf) * mf + tab[ti + 1, mi + 1] * tf * mf)


def vortex(p, centre, radius, twist):
    """Rotate points p about the axis `centre` by an angle that falls off with angular distance (a cyclone)."""
    c = np.array(centre, np.float32)
    c = c / np.linalg.norm(c)
    dot = np.clip((p * c).sum(-1), -1, 1)
    ang = np.arccos(dot)
    phi = (twist * np.exp(-(ang / radius) ** 2))[..., None]
    cross = np.cross(np.broadcast_to(c, p.shape), p)
    return p * np.cos(phi) + cross * np.sin(phi) + c * (dot[..., None] * (1 - np.cos(phi)))


def weather_clouds(p, lat, seed, cover, vortices=4, zonal=0.35, wisp=0.5, tropical=True, sharp=1.0):
    """Earth-like cloud deck: cyclonic swirls, ITCZ and storm-track banding, fine wisps; thin edges, clear patches."""
    rng = np.random.default_rng(seed)
    q = p
    for _ in range(vortices):
        la = rng.uniform(0.35, 1.1) * rng.choice([-1, 1])
        lo = rng.uniform(0, 2 * np.pi)
        c = (math.cos(la) * math.cos(lo), math.sin(la), math.cos(la) * math.sin(lo))
        q = vortex(q, c, rng.uniform(0.2, 0.42), rng.uniform(2.0, 4.0) * (1 if la > 0 else -1))
    q = domain_warp(q, seed + 3, 0.35, 1.6, 3)
    base = fbm(q * np.array([1.0, 1.7, 1.0], np.float32), seed + 5, 7, 2.0, gain=0.58)
    zon = np.cos(6 * lat)[:, None] if tropical else np.cos(4 * lat)[:, None]
    f = base + zonal * 0.5 * zon * 0.5
    thr = np.percentile(f[::4, ::4], (1 - cover) * 100)
    edge = 0.045 / sharp
    c = smooth(thr - edge, thr + edge * 2.5, f)
    detail = fbm(q, seed + 9, 6, 8.0, gain=0.6)
    c = c * (1 - wisp + wisp * smooth(0.25, 0.7, detail) * 1.25)
    # thin cirrus streaks where the cover is low
    streak = smooth(0.62, 0.78, fbm(q * np.array([1.0, 3.0, 1.0], np.float32), seed + 11, 5, 5.0)) * 0.35
    return np.clip(np.maximum(c, streak * (1 - c)), 0, 1).astype(np.float32)


def night_lights(p, land_mask, seed, density, color=(1.0, 0.78, 0.45)):
    clus = norm01(fbm(p, seed + 40, 5, 3.0))
    spots = norm01(fbm(p, seed + 41, 4, 28.0))
    m = land_mask * smooth(0.55, 0.9, clus) * smooth(1 - density, 1.0, spots)
    return (m[..., None] * np.array(color, np.float32)[None, None, :]).astype(np.float32)


def terrain(p, lat, seed, ocean_frac, cont_freq=1.15, mount_amt=0.5):
    """Continents with fractal coastlines and orogenic ridges. Returns (elevation e 0..1, sea level, land el 0..1, mountains)."""
    q = domain_warp(p, seed + 1, 0.38, 1.3, 3)
    cont = fbm(q, seed, 7, cont_freq, gain=0.5)
    cont = norm01(cont, 0.5, 99.5)
    sea = float(np.percentile(cont[::4, ::4], ocean_frac * 100))
    el = np.clip((cont - sea) / max(1 - sea, 1e-3), 0, 1)
    mount = fbm(domain_warp(p, seed + 7, 0.25, 2.5, 3), seed + 5, 7, 3.2, ridged=True)
    mmask = smooth(0.12, 0.5, el) * smooth(0.35, 0.65, fbm(p, seed + 6, 4, 2.0))
    mount = mount * mmask
    return cont, sea, el, mount


def ocean_colour(cont, sea, extra_shallow=0.0):
    d = np.clip((sea - cont) / max(sea, 1e-3) * 2.4, 0, 1)
    return ramp(d, [(0.0, hexc('#46a4ac')), (0.03 + extra_shallow, hexc('#2b86a3')), (0.10, hexc('#185c92')),
                    (0.28, hexc('#0d3a76')), (0.6, hexc('#082a5c')), (1.0, hexc('#061c46'))])


EARTH_TABLE = [  # temperature rows (frozen, cold, temperate, hot) x moisture columns (dry .. wet), display sRGB
    ['#dde3e6', '#e2e7ea', '#e8ecee', '#eef1f2'],
    ['#8f8a7c', '#7c7c64', '#475a3f', '#34493a'],
    ['#b3a276', '#8f8e5e', '#4a6438', '#33502e'],
    ['#c1a373', '#9c9158', '#436a2b', '#27471f'],
]
JUNGLE_TABLE = [
    ['#dde3e6', '#e2e7ea', '#e8ecee', '#eef1f2'],
    ['#7d7a60', '#5f6a46', '#3d5330', '#2f4829'],
    ['#8d8a55', '#667c39', '#3d6128', '#2a4d20'],
    ['#9a8a4c', '#6b8035', '#37622a', '#1f4a1d'],
]


def earthlike(p, lat, seed, ocean_frac, table, warm, wet, ice_lat=0.86, cloud_cover=0.5, cities=0.35, jungle=False, lights_color=(1.0, 0.78, 0.45)):
    cont, sea, el, mount = terrain(p, lat, seed, ocean_frac)
    land = cont > sea
    sl = np.abs(np.sin(lat))[:, None]
    n_t = fbm(p, seed + 20, 5, 3.0) - 0.5
    T = warm - 1.0 * sl ** 1.5 - 0.62 * el - 0.45 * mount + 0.12 * n_t
    zon = 0.5 + 0.5 * np.cos(6 * lat)[:, None]
    Mo = np.clip(0.18 + wet * (0.55 * zon + 0.55 * fbm(p, seed + 22, 6, 2.6)) - 0.2 * mount, 0, 1)
    # rain shadow / drier continental interiors: moisture falls with distance from the sea
    inland = ndimage.gaussian_filter(np.where(land, 1.0, 0.0).astype(np.float32), 18, mode='wrap')
    Mo = np.clip(Mo - 0.35 * (inland - 0.5) * (1 if not jungle else 0.4), 0, 1)
    land_c = bilinear_table(T, Mo, table)
    # fine variegation of vegetation and soil
    var = fbm(p, seed + 24, 6, 14.0)
    land_c = land_c * (0.82 + 0.36 * var)[..., None]
    # bare mountain rock and scree, snow above the snowline
    rock = ramp(np.clip(mount * 1.6, 0, 1), [(0, hexc('#6c6256')), (1, hexc('#8d8378'))])
    rm = smooth(0.35, 0.8, mount)[..., None]
    land_c = land_c * (1 - rm * 0.75) + rock * rm * 0.75
    snow = smooth(0.16, 0.06, T)[..., None]
    snow = np.maximum(snow, smooth(0.55, 0.9, mount) * smooth(0.55, 0.3, T)[..., None]) if False else snow
    land_c = land_c * (1 - snow) + col('#eef1f3') * snow
    ocean = ocean_colour(cont, sea, 0.0)
    # sea ice and polar caps
    ice = smooth(ice_lat - 0.04, ice_lat + 0.05, sl + 0.07 * n_t)
    alb = np.where(land[..., None], land_c, ocean)
    alb = alb * (1 - ice[..., None]) + col('#f1f4f6') * ice[..., None]
    height = np.where(land, 0.012 * el + 0.02 * mount, -0.004 * (1 - np.clip(cont / max(sea, 1e-3), 0, 1)))
    rough = np.where(land, 0.88, 0.16)
    rough = np.where(ice > 0.5, 0.55, rough) + np.where(land, 0.0, 0.0)
    snowm = (snow[..., 0] > 0.5) & land
    rough = np.where(snowm, 0.6, rough)
    clouds = weather_clouds(p, lat, seed + 30, cloud_cover)
    nl = night_lights(p, (land & (ice < 0.3) & (T > 0.2)).astype(np.float32) * (1 - 0.7 * smooth(0.55, 0.9, mount)), seed, cities, lights_color)
    return alb, height, clouds, nl, rough.astype(np.float32)


def look_m(p, lat, v):
    return earthlike(p, lat, v, 0.57, EARTH_TABLE, warm=1.12, wet=1.0, ice_lat=0.94, cloud_cover=0.26)


def look_l(p, lat, v):
    a, h, c, n, r = earthlike(p, lat, v, 0.38, JUNGLE_TABLE, warm=1.45, wet=1.2, ice_lat=0.97, cloud_cover=0.36, cities=0.25, jungle=True)
    # marginal world: murky teal seas, a slight green-yellow haze in the colour balance
    a = a * np.array([1.0, 1.02, 0.94], np.float32)
    return a, h, c, n, r


def look_o(p, lat, v):
    cont, sea, el, mount = terrain(p, lat, v, 0.93, cont_freq=1.5)
    land = cont > sea
    sl = polar(lat)
    n_t = fbm(p, v + 20, 5, 3.0) - 0.5
    ocean = ocean_colour(cont, sea, 0.05)
    # tropical reef shelves: bright turquoise around every island
    reef = smooth(0.0, 0.25, 1 - np.clip((sea - cont) / (0.07 * sea), 0, 1)) * (~land)
    ocean = ocean * (1 - reef[..., None] * 0.7) + col('#4cc0c0') * reef[..., None] * 0.7
    T = 0.95 - 1.0 * sl ** 1.5 - 0.5 * el + 0.1 * n_t
    isl = bilinear_table(T, np.full_like(T, 0.8), EARTH_TABLE) * (0.85 + 0.3 * fbm(p, v + 24, 5, 14.0))[..., None]
    beach = smooth(0.0, 0.1, el)[..., None]
    isl = col('#dccd9e') * (1 - beach) + isl * beach
    ice = smooth(0.93, 0.975, sl + 0.04 * n_t)
    alb = np.where(land[..., None], isl, ocean)
    alb = alb * (1 - ice[..., None]) + col('#eef2f5') * ice[..., None]
    rough = np.where(land, 0.85, 0.13)
    rough = np.where(ice > 0.5, 0.55, rough).astype(np.float32)
    height = np.where(land, 0.01 * el, -0.003)
    clouds = weather_clouds(p, lat, v + 30, 0.32, vortices=5)
    nl = night_lights(p, (land & (ice < 0.3)).astype(np.float32), v, 0.2)
    return alb, height, clouds, nl, rough


def look_p(p, lat, v):
    """Glacial world: Europa / Antarctic ice. Bright blue-white ice, tan-brown lineae, chaos terrain, bright-rimmed craters."""
    sl = polar(lat)
    q = domain_warp(p, v + 2, 0.25, 1.5, 3)
    base = fbm(q, v, 7, 2.0)
    ice = ramp(norm01(base), [(0, hexc('#a9bfd2')), (0.5, hexc('#cddbe7')), (1, hexc('#eef5fa'))])
    # lineae: long ridged cracks, brown-tan stain with a bluish halo
    ln = np.zeros(sl.shape[:1] + p.shape[1:2], np.float32)
    for k, (f, w) in enumerate([(1.1, 0.075), (2.2, 0.05), (4.5, 0.032)]):
        g = np.abs(gnoise(domain_warp(p, v + 10 + k, 0.2, 1.2, 3), f, v + 33 + k))
        ln = np.maximum(ln, smooth(w, 0.0, g) * (0.9 - 0.2 * k))
    stain = ramp(np.clip(ln, 0, 1), [(0, hexc('#e3edf4')), (1, hexc('#8a6a52'))])
    # chaos terrain: mottled darker, tan-blue patches
    ch = smooth(0.58, 0.72, fbm(q, v + 6, 5, 2.4)) * smooth(0.45, 0.75, fbm(p, v + 8, 6, 9.0))
    chaos = ramp(norm01(fbm(p, v + 9, 6, 16.0)), [(0, hexc('#7f6d62')), (1, hexc('#b9bcc0'))])
    # exposed dark bedrock massifs
    h = norm01(fbm(p, v + 3, 8, 2.0))
    rockm = smooth(0.72, 0.9, h) * smooth(0.5, 0.2, sl * 1.0 + 0.0)
    rock = ramp(norm01(fbm(p, v + 5, 6, 6.0)), [(0, hexc('#4a525c')), (1, hexc('#7a838c'))])
    alb = ice * (1 - ln[..., None] * 0.85) + stain * ln[..., None] * 0.85
    alb = alb * (1 - ch[..., None] * 0.55) + chaos * ch[..., None] * 0.55
    alb = alb * (1 - rockm[..., None] * 0.8) + rock * rockm[..., None] * 0.8
    hgt = 0.01 * h + 0.004 * (1 - ln)
    bright = np.ones_like(hgt)
    hgt = hgt.astype(np.float32)
    stamp_craters(p, lat, hgt, bright, v + 50, 700, 0.006, 0.07, 1.8, depth_k=0.12, complex_r=0.03, region_weight=1 - 0.5 * ch)
    alb = alb * np.clip(bright, 0.6, 1.5)[..., None]
    alb = np.clip(alb, 0, 1)
    rough = np.where(ln > 0.4, 0.6, 0.45).astype(np.float32)
    clouds = weather_clouds(p, lat, v + 30, 0.26, vortices=2, wisp=0.7)
    nl = night_lights(p, ((sl < 0.6) & (h < 0.4)).astype(np.float32)[..., 0] if False else ((sl < 0.6) * (h < 0.4)).astype(np.float32), v, 0.1, (0.85, 0.92, 1.0))
    return alb, hgt, clouds, nl, rough


def look_h(p, lat, v):
    """Hot desert world: Mars. Rust dust plains, dark basalt basins, cratered highlands, a long canyon, thin polar caps."""
    sl = polar(lat)
    q = domain_warp(p, v + 2, 0.3, 1.2, 3)
    cont = norm01(fbm(q, v, 7, 1.3))
    dark = smooth(0.46, 0.64, fbm(domain_warp(p, v + 4, 0.35, 1.0, 3), v + 5, 6, 1.5)) * smooth(0.0, 0.25, 1 - np.clip(np.abs(lat)[:, None] / 1.2, 0, 1) + 0.15)
    detail = fbm(p, v + 6, 7, 6.0)
    dust = ramp(norm01(fbm(q, v + 7, 6, 3.0)), [(0, hexc('#a85d33')), (0.5, hexc('#c27a46')), (1, hexc('#d99a62'))])
    basalt = ramp(norm01(fbm(p, v + 8, 6, 5.0)), [(0, hexc('#3f3029')), (1, hexc('#6a4b3b'))])
    alb = dust * (1 - dark[..., None] * 0.92) + basalt * dark[..., None] * 0.92
    alb = alb * (0.86 + 0.28 * detail)[..., None]
    # wind streaks
    streak = fbm(p * np.array([1.0, 2.0, 1.0], np.float32), v + 9, 5, 12.0)
    alb = alb * (0.92 + 0.16 * streak)[..., None]
    hgt = (0.02 * cont + 0.006 * detail).astype(np.float32)
    # Valles-like canyon system: a long warped trough
    cl = np.abs(gnoise(domain_warp(p, v + 12, 0.15, 1.0, 3), 1.6, v + 70))
    canyon = smooth(0.035, 0.0, cl) * smooth(0.5, 0.8, fbm(p, v + 14, 4, 1.2)) * smooth(0.8, 0.4, sl)
    hgt -= 0.012 * canyon
    alb = alb * (1 - 0.35 * canyon[..., None]) + col('#5a3a2c') * 0.35 * canyon[..., None]
    # craters with darkened floors and bright ejecta
    bright = np.ones_like(hgt)
    stamp_craters(p, lat, hgt, bright, v + 50, 2600, 0.005, 0.11, 1.9, depth_k=0.18, complex_r=0.04, region_weight=1 - 0.55 * dark)
    alb = alb * np.clip(bright, 0.65, 1.6)[..., None]
    # polar caps: thin, white with a pink tint
    cap = smooth(0.90, 0.95, sl + 0.05 * (detail - 0.5))
    alb = alb * (1 - cap[..., None] * 0.92) + col('#f2e8e4') * cap[..., None] * 0.92
    alb = np.clip(alb, 0, 1)
    rough = np.where(cap > 0.5, 0.5, 0.92).astype(np.float32)
    clouds = weather_clouds(p, lat, v + 30, 0.14, vortices=1, wisp=0.8, sharp=1.4)
    clouds = clouds * (0.35 + 0.65 * smooth(0.55, 0.9, sl)) * 0.7  # thin, mostly polar hood and orographic wisps
    nl = night_lights(p, ((1 - dark) * (sl < 0.75)).astype(np.float32), v, 0.1)
    return alb, hgt, clouds, nl, rough


def look_k(p, lat, v):
    """Airless cratered moon: bright highlands, dark basalt maria, a real crater size distribution, rayed young craters."""
    H, W = lat.shape[0], p.shape[1]
    # maria: a handful of huge basins with ragged edges, mostly on one hemisphere
    rng = np.random.default_rng(v)
    mare = np.zeros((H, W), np.float32)
    for i in range(9):
        la = rng.uniform(-0.8, 0.8)
        lo = rng.normal(0.0, 1.1)  # centred on lon 0: a near side
        c = np.array([math.cos(la) * math.cos(lo), math.sin(la), math.cos(la) * math.sin(lo)], np.float32)
        r = rng.uniform(0.16, 0.42)
        ang = np.arccos(np.clip((p * c).sum(-1), -1, 1))
        mare = np.maximum(mare, smooth(r, r * 0.6, ang + 0.11 * (fbm(p, v + 80 + i, 5, 3.0) - 0.5) * r * 6))
    mare = mare * smooth(0.28, 0.5, fbm(p, v + 90, 4, 2.0) + 0.25 * mare)
    hgt = (0.004 * (fbm(p, v, 7, 3.0) - 0.5) - 0.006 * mare).astype(np.float32)
    bright = np.ones((H, W), np.float32)
    weight = 1.0 - 0.8 * mare
    stamp_craters(p, lat, hgt, bright, v + 50, 9000, 0.0042, 0.16, 1.75, depth_k=0.2, complex_r=0.05, fresh_rays=2, region_weight=weight)
    hl = ramp(norm01(fbm(p, v + 8, 7, 4.0)), [(0, hexc('#6f6a64')), (0.5, hexc('#86807a')), (1, hexc('#9d978f'))])
    mr = ramp(norm01(fbm(p, v + 9, 6, 2.6)), [(0, hexc('#2c2927')), (0.5, hexc('#3b3836')), (1, hexc('#4d4946'))])
    # warm/cool basalt (titanium rich maria are bluer, others redder)
    mr = mr * (1 + 0.08 * (fbm(p, v + 12, 4, 1.7)[..., None] - 0.5) * np.array([1.5, 0, -1.5], np.float32))
    alb = hl * (1 - mare[..., None]) + mr * mare[..., None]
    alb = alb * np.clip(bright, 0.6, 1.45)[..., None] * (0.9 + 0.2 * fbm(p, v + 13, 6, 18.0))[..., None]
    alb = np.clip(alb, 0, 1)
    rough = np.full(hgt.shape, 0.95, np.float32)
    return alb, hgt, None, None, rough


def look_y(p, lat, v):
    """Demon world: basalt crust with a glowing lava crack network, sulphur patches, lava lakes, a thick amber haze."""
    sl = polar(lat)
    q = domain_warp(p, v + 2, 0.3, 1.5, 4)
    h = norm01(fbm(q, v, 8, 2.0))
    crust = ramp(norm01(fbm(p, v + 4, 7, 5.0)), [(0, hexc('#120c0a')), (0.5, hexc('#2a1c16')), (1, hexc('#4a3226'))])
    sulf = smooth(0.6, 0.78, fbm(domain_warp(p, v + 6, 0.3, 2.0, 3), v + 7, 6, 3.0)) * smooth(0.55, 0.3, h)
    sulfc = ramp(norm01(fbm(p, v + 8, 5, 9.0)), [(0, hexc('#8d7a2a')), (1, hexc('#c0a840'))])
    alb = crust * (1 - sulf[..., None] * 0.55) + sulfc * sulf[..., None] * 0.55
    # lava: ridged fracture lines (thin) at three scales, plus lakes in the lowlands
    lines = np.zeros_like(h)
    for k, (f, w) in enumerate([(1.4, 0.085), (3.0, 0.06), (6.5, 0.04)]):
        g = np.abs(gnoise(domain_warp(p, v + 20 + k, 0.25, 1.4, 3), f, v + 60 + k))
        lines = np.maximum(lines, smooth(w, 0.0, g) * (1.0 - 0.15 * k))
    lakes = smooth(0.30, 0.20, h) * smooth(0.45, 0.65, fbm(p, v + 30, 5, 3.0))
    lava = np.clip(np.maximum(lines * smooth(0.25, 0.6, fbm(p, v + 31, 4, 2.0)), lakes), 0, 1)
    glow_core = smooth(0.35, 0.95, lava)
    lava_c = ramp(lava, [(0, hexc('#2a0e08')), (0.4, hexc('#a82a0c')), (0.75, hexc('#ec6a18')), (1, hexc('#ffd06a'))])
    alb = alb * (1 - lava[..., None] * 0.85) + lava_c * lava[..., None] * 0.85
    alb = np.clip(alb * (0.85 + 0.3 * fbm(p, v + 32, 6, 14.0))[..., None], 0, 1)
    night = (ramp(lava, [(0, (0, 0, 0)), (0.3, hexc('#7a1a05')), (0.7, hexc('#ff6a14')), (1, hexc('#ffc060'))]) * (lava[..., None] ** 0.8)).astype(np.float32)
    hgt = (0.02 * h - 0.008 * lava).astype(np.float32)
    rough = np.where(lava > 0.4, 0.35, 0.9).astype(np.float32)
    clouds = weather_clouds(p, lat, v + 30, 0.3, vortices=3, wisp=0.7, sharp=0.9)
    return alb, hgt, clouds, night, rough


# --- gas giant --------------------------------------------------------------------------------------------------

GAS_SETS = {
    'jupiter': dict(stops=[(0, '#58321f'), (0.18, '#a5603a'), (0.38, '#d7a874'), (0.58, '#efe0c4'), (0.78, '#f6efe0'), (1, '#c9a577')],
                    storm=('#c4552f', 0.9), nb=11, contrast=1.0, pole='#7d8aa0'),
    'saturn': dict(stops=[(0, '#9c7f4e'), (0.3, '#c9ad76'), (0.6, '#e3cf9c'), (0.85, '#efe0b6'), (1, '#f5ebcc')],
                   storm=('#d8c08a', 0.4), nb=14, contrast=0.55, pole='#8c9aa8'),
    'neptune': dict(stops=[(0, '#1d3a86'), (0.3, '#2f58b0'), (0.6, '#4d82d6'), (0.85, '#8db4ec'), (1, '#e3eefc')],
                    storm=('#14265c', 0.8), nb=9, contrast=0.8, pole='#3a62b0'),
    'ice': dict(stops=[(0, '#6fbcc2'), (0.4, '#8fd0d2'), (0.75, '#a8dfdf'), (1, '#c2ecea')],
                storm=('#5aa8b0', 0.3), nb=8, contrast=0.3, pole='#78c0c6'),
    'rust': dict(stops=[(0, '#3e1c12'), (0.2, '#7d3a22'), (0.42, '#b9683e'), (0.65, '#e4a878'), (0.85, '#f0d0a8'), (1, '#9a5a38')],
                 storm=('#e8c8a0', 0.8), nb=12, contrast=1.1, pole='#6b5a64'),
    'slate': dict(stops=[(0, '#26363c'), (0.3, '#46666a'), (0.6, '#8aaa9f'), (0.85, '#c2d4c6'), (1, '#e4ecda')],
                  storm=('#e8f0e0', 0.8), nb=10, contrast=0.9, pole='#5a7380'),
}
GAS_ORDER = ['jupiter', 'saturn', 'neptune', 'ice', 'rust', 'slate']


def band_profile(H, nb, seed):
    """Row profile: nb belts/zones of random width and tone with sharp-ish edges (not a sine, which reads as wood grain)."""
    rng = np.random.default_rng(seed)
    widths = rng.uniform(0.55, 1.45, nb)
    edges = np.concatenate([[0], np.cumsum(widths)]) / widths.sum()
    tone = np.where(np.arange(nb) % 2 == 0, rng.uniform(0.7, 1.0, nb), rng.uniform(0.0, 0.32, nb))
    y = (np.arange(H) + 0.5) / H
    prof = np.interp(y, (edges[:-1] + edges[1:]) / 2, tone)
    # step-like: pick the band tone by index with a narrow blend at the edges
    idx = np.clip(np.searchsorted(edges, y) - 1, 0, nb - 1)
    prof = tone[idx]
    prof = ndimage.gaussian_filter1d(prof, H / 220.0, mode='nearest')
    # zonal speed: alternating by band, strongest at the edges between them
    u = np.where(idx % 2 == 0, 1.0, -1.0) * (0.55 + 0.45 * rng.uniform(0, 1, nb)[idx])
    u = ndimage.gaussian_filter1d(u, H / 60.0, mode='nearest')
    return prof.astype(np.float32), u.astype(np.float32)


def look_gas(p, lat, v):
    H, W = lat.shape[0], p.shape[1]
    vi = ((v - 100) % 1000) // 17  # variant 1..6
    kind = GAS_ORDER[(vi - 1) % 6]
    cfg = GAS_SETS[kind]
    # dynamics at 1024 wide, detail re-added at full width
    sw, sh = W // 2, H // 2
    ps, lats = sphere_dirs(sw, sh)
    prof, u = band_profile(sh, cfg['nb'], v)
    wob = fbm(ps * np.array([1, 0.1, 1], np.float32), v + 1, 4, 3.0) - 0.5
    t0 = np.clip(prof[:, None] + wob * 0.25 * cfg['contrast'], 0, 1).astype(np.float32)
    # a second tracer for haze brightness
    t1 = (fbm(ps * np.array([1, 0.2, 1], np.float32), v + 2, 5, 4.0)).astype(np.float32)
    shear = np.abs(np.gradient(u))
    shear = (shear / max(shear.max(), 1e-6)).astype(np.float32)
    shear = ndimage.gaussian_filter1d(shear, 2.0)
    shear = 0.25 + 0.75 * shear / max(shear.max(), 1e-6)
    taper = np.cos(lats * 1.0) ** 0.5  # flow dies toward the poles
    uu = u * taper * 0.012
    t0, t1 = advect([t0, t1], uu, 0.9 * taper * shear, shear, 90, 1.0, v + 3, psi_freq=3.5)
    # upscale and add streaky fine structure along the flow
    t0 = np.asarray(Image.fromarray(t0).resize((W, H), Image.LANCZOS))
    t1 = np.asarray(Image.fromarray(t1).resize((W, H), Image.LANCZOS))
    streak = fbm(p * np.array([1, 0.12, 1], np.float32), v + 5, 6, 9.0) - 0.5
    fine = fbm(p * np.array([1, 0.3, 1], np.float32), v + 6, 5, 22.0) - 0.5
    t = np.clip(0.5 + (t0 - 0.5) * 1.35 + (0.16 * streak + 0.08 * fine) * cfg['contrast'] + 0.05 * (t1 - 0.5), 0, 1)
    # polar mottling in 3-D space (no stretching at the poles)
    pol = smooth(0.62, 0.9, polar(lat))
    mott = fbm(p, v + 7, 6, 7.0)
    t = t * (1 - pol) + (0.35 + 0.4 * mott) * pol
    stops = [(a, hexc(b)) for a, b in cfg['stops']]
    alb = ramp(t, stops)
    alb = alb * (1 - pol[..., None] * 0.5) + col(cfg['pole']) * pol[..., None] * 0.5
    # storms: a big oval with a spiral collar, plus small white ovals
    rng = np.random.default_rng(v + 9)
    lon_g = (np.arange(W) + 0.5) / W * 2 * np.pi
    lat_g = lat[:, None]
    storms = [(rng.uniform(-0.45, -0.2), rng.uniform(0, 2 * np.pi), 0.34, 0.16, cfg['storm'][0], cfg['storm'][1])]
    for _ in range(3):
        storms.append((rng.uniform(-0.5, 0.5), rng.uniform(0, 2 * np.pi), 0.07, 0.04, '#f6f2ea', 0.8))
    for la, lo, rx, ry, c, amp in storms:
        dlon = (lon_g[None, :] - lo + np.pi) % (2 * np.pi) - np.pi
        dx = dlon * np.cos(la) / rx
        dy = (lat_g - la) / ry
        r = np.sqrt(dx * dx + dy * dy)
        inside = np.exp(-(np.clip(r, 0, 3) ** 3) * 0.9)
        collar = smooth(1.5, 1.0, r) * smooth(0.7, 1.0, r)
        swirl = 0.5 + 0.5 * np.sin(np.arctan2(dy, dx) * 2 + r * 8)
        alb = alb * (1 - inside[..., None] * amp) + col(c) * (inside * (0.85 + 0.2 * swirl))[..., None] * amp
        alb = alb * (1 + 0.12 * collar[..., None] * (swirl[..., None] - 0.5))
    rough = np.full((H, W), 0.55, np.float32)
    hgt = np.zeros((H, W), np.float32)
    return np.clip(alb, 0, 1).astype(np.float32), hgt, None, None, rough


LOOKS = {'m': look_m, 'o': look_o, 'p': look_p, 'l': look_l, 'h': look_h, 'k': look_k, 'y': look_y, 'gasgiant': look_gas}
# relief exaggeration for the normal maps (1 = true geometry for a map whose heights are in planet radii)
NORMAL_EXAG = {'m': 6, 'o': 6, 'p': 5, 'l': 6, 'h': 3.5, 'k': 2.6, 'y': 5}


def to8(a):
    return (np.clip(a, 0, 1) * 255 + 0.5).astype(np.uint8)


def up(img8, size):
    return np.asarray(Image.fromarray(img8).resize(size, Image.LANCZOS))


def sharpen(a8, amount=0.7, radius=1.2):
    im = Image.fromarray(a8).filter(ImageFilter.UnsharpMask(radius=radius, percent=int(amount * 100), threshold=0))
    return np.asarray(im)


def make_variant(args):
    cls, v, work_w = args
    dd = os.path.join(OUT, cls, 'desktop')
    wd = os.path.join(OUT, cls, 'web')
    os.makedirs(dd, exist_ok=True)
    os.makedirs(wd, exist_ok=True)
    W, H = work_w, work_w // 2
    p, lat = sphere_dirs(W, H)
    seed = 100 + 17 * v + 1000 * CLASSES.index(cls)
    alb, height, cl, nl, rough = LOOKS[cls](p, lat, seed)
    alb8 = to8(alb)
    full = (4096, 2048)
    big = up(alb8, full)
    big = sharpen(big, 0.55, 1.3)
    Image.fromarray(big).save(os.path.join(dd, 'v%d_albedo.png' % v), compress_level=3)
    px_per_rad = W / (2 * math.pi)
    normal = None
    if cls in NORMAL_EXAG:
        n8 = normal_from_height(height.astype(np.float32), px_per_rad, NORMAL_EXAG[cls])
        normal = up(n8, (2048, 1024))
        Image.fromarray(normal).save(os.path.join(dd, 'v%d_normal.png' % v))
    Image.fromarray(up(to8(rough), (2048, 1024)) if rough.shape[1] != 2048 else to8(rough), 'L').save(os.path.join(dd, 'v%d_rough.png' % v))
    if cl is not None:
        Image.fromarray(up(to8(cl), (2048, 1024)), 'L').save(os.path.join(dd, 'v%d_clouds.png' % v))
    if nl is not None:
        Image.fromarray(up(to8(nl), (2048, 1024))).save(os.path.join(dd, 'v%d_night.png' % v))
    if v <= 4:
        small = np.asarray(Image.fromarray(alb8).resize((1024, 512), Image.LANCZOS)).astype(np.float32) / 255
        if cls in NORMAL_EXAG:  # bake relief into the web albedo
            sh = shade_from_height(height.astype(np.float32), px_per_rad, NORMAL_EXAG[cls] * 1.2)
            sh = np.asarray(Image.fromarray(sh.astype(np.float32)).resize((1024, 512), Image.BILINEAR))
            small = small * np.clip(sh, 0.45, 1.5)[..., None]
        Image.fromarray(to8(small)).save(os.path.join(wd, 'v%d_albedo.webp' % v), quality=84, method=4)
        Image.fromarray(up(to8(rough), (512, 256)), 'L').save(os.path.join(wd, 'v%d_rough.png' % v))
        if cl is not None:
            Image.fromarray(to8(cl), 'L').resize((512, 256), Image.LANCZOS).save(os.path.join(wd, 'v%d_clouds.png' % v))
        if nl is not None:
            Image.fromarray(to8(nl)).resize((512, 256), Image.LANCZOS).save(os.path.join(wd, 'v%d_night.png' % v))
    return cls, v


# --- asteroids ----------------------------------------------------------------------------------------------------

AST_TYPES = [  # (name, albedo ramp low, mid, high): S-type stony brown-grey, C-type carbonaceous charcoal, M-type grey metal
    ('S', '#3f3a35', '#6f665c', '#a09585'),
    ('C', '#1f1d1b', '#3a3632', '#5e574f'),
    ('S', '#463f38', '#786d60', '#ab9f8e'),
    ('M', '#363636', '#62605d', '#8e8a84'),
    ('C', '#272320', '#443e38', '#6b6258'),
    ('S', '#3a3631', '#675f56', '#938a7d'),
]


def make_asteroids():
    d = os.path.join(OUT, 'asteroids')
    os.makedirs(os.path.join(d, 'desktop'), exist_ok=True)
    os.makedirs(os.path.join(d, 'web'), exist_ok=True)
    for old in ('rock_albedo.png', 'rock_normal.png'):
        pth = os.path.join(d, 'desktop', old)
        if os.path.exists(pth):
            os.remove(pth)  # superseded by per-rock maps (the old file was a generated artefact, not source)
    W, H = 1024, 512
    p, lat = sphere_dirs(W, H)
    for i in range(6):
        seed = 900 + 13 * i
        rng = np.random.default_rng(seed)
        # shape: low-frequency lumpy ellipsoid (potato) with big craters carved into it
        stretch = np.array([1.0, rng.uniform(0.55, 0.9), rng.uniform(0.65, 0.95)], np.float32)
        lump = fbm(domain_warp(p, seed + 1, 0.3, 1.0, 3), seed, 4, 1.4) - 0.5
        hgt = (0.5 * lump + 0.06 * (fbm(p, seed + 2, 6, 4.0) - 0.5)).astype(np.float32)
        bright = np.ones_like(hgt)
        stamp_craters(p, lat, hgt, bright, seed + 3, 650, 0.03, 0.55, 1.3, depth_k=0.3, complex_r=0.5, fresh_rays=0)
        stamp_craters(p, lat, hgt, bright, seed + 4, 2500, 0.01, 0.12, 2.0, depth_k=0.28, complex_r=0.5)
        name, lo, mid, hi = AST_TYPES[i]
        t = norm01(fbm(p, seed + 6, 7, 3.0))
        alb = ramp(t, [(0, hexc(lo)), (0.5, hexc(mid)), (1, hexc(hi))])
        mid_c = np.array(hexc(mid), np.float32)
        alb = mid_c + 0.38 * (alb - mid_c)  # one rock is one material: low albedo contrast
        alb = alb * (0.88 + 0.24 * fbm(p, seed + 7, 6, 12.0))[..., None]
        # albedo contrast: fresh crater walls and rims brighter, floors (settled regolith) darker; bright slope streaks
        slope = np.hypot(*np.gradient(hgt)) * W
        alb = alb * (1 + 0.25 * np.clip(slope * 0.6, 0, 1.0) - 0.08)[..., None] * np.clip(bright, 0.8, 1.5)[..., None]
        alb = np.clip(alb * 0.55, 0, 1)  # real asteroid albedo is 0.04 to 0.25: under a hard key they must not blow out
        Image.fromarray(to8(alb)).save(os.path.join(d, 'desktop', 'rock_%d_albedo.png' % (i + 1)))
        Image.fromarray(normal_from_height(hgt, W / (2 * math.pi), 0.22)).save(os.path.join(d, 'desktop', 'rock_%d_normal.png' % (i + 1)))
        Image.fromarray(to8(alb)).resize((512, 256), Image.LANCZOS).save(os.path.join(d, 'web', 'rock_%d_albedo.webp' % (i + 1)), quality=80)
        for tag, (nu, nv) in (('desktop', (96, 48)), ('web', (40, 20))):
            write_rock_obj(os.path.join(d, tag, 'rock_%d.obj' % (i + 1)), hgt, stretch, nu, nv, i + 1)


def write_rock_obj(path, hgt, stretch, nu, nv, idx):
    """UV sphere (seam column duplicated so the UVs wrap cleanly) displaced by the same height map the maps came from."""
    H, W = hgt.shape
    # the mesh carries only the large forms: the small craters live in the normal map (sampling them per vertex is noise)
    hgt = ndimage.gaussian_filter(hgt, sigma=(5 * H / 512, 5 * W / 1024), mode='wrap') * 1.6
    verts, uvs, normals = [], [], []
    grid = np.zeros((nv + 1, nu + 1, 3), np.float32)
    for j in range(nv + 1):
        la = (0.5 - j / nv) * np.pi
        for i in range(nu + 1):
            lo = i / nu * 2 * np.pi
            d = np.array([math.cos(la) * math.cos(lo), math.sin(la), math.cos(la) * math.sin(lo)], np.float32)
            x = min(int((i / nu) * W), W - 1)
            y = min(int(j / nv * H), H - 1)
            r = 1.0 + 0.9 * float(hgt[y, x])
            grid[j, i] = d * max(r, 0.35) * stretch
    sc = np.abs(grid).max()
    grid /= sc
    grid[:, nu] = grid[:, 0]  # exact seam
    # smooth normals from the grid (merge by position across the seam)
    nrm = np.zeros_like(grid)
    for j in range(nv):
        for i in range(nu):
            a, b, c, e = grid[j, i], grid[j, i + 1], grid[j + 1, i], grid[j + 1, i + 1]
            n1 = np.cross(b - a, c - a)
            n2 = np.cross(e - b, c - b)
            for (jj, ii) in ((j, i), (j, i + 1), (j + 1, i)):
                nrm[jj, ii] += n1
            for (jj, ii) in ((j, i + 1), (j + 1, i), (j + 1, i + 1)):
                nrm[jj, ii] += n2
    nrm[:, 0] += nrm[:, nu]
    nrm[:, nu] = nrm[:, 0]
    nrm[0] = nrm[0].sum(0)
    nrm[nv] = nrm[nv].sum(0)
    nrm /= np.maximum(np.linalg.norm(nrm, axis=-1, keepdims=True), 1e-9)
    with open(path, 'w') as fh:
        fh.write('# L9 procedural asteroid %d (original work): cratered S/C/M-type rock\n' % idx)
        for j in range(nv + 1):
            for i in range(nu + 1):
                fh.write('v %.5f %.5f %.5f\n' % tuple(grid[j, i]))
        for j in range(nv + 1):
            for i in range(nu + 1):
                fh.write('vt %.5f %.5f\n' % (i / nu, 1 - j / nv))
        for j in range(nv + 1):
            for i in range(nu + 1):
                fh.write('vn %.5f %.5f %.5f\n' % tuple(nrm[j, i]))
        for j in range(nv):
            for i in range(nu):
                a = j * (nu + 1) + i + 1
                b = a + 1
                c = a + nu + 1
                e = c + 1
                if j > 0:
                    fh.write('f %d/%d/%d %d/%d/%d %d/%d/%d\n' % (a, a, a, b, b, b, c, c, c))
                if j < nv - 1:
                    fh.write('f %d/%d/%d %d/%d/%d %d/%d/%d\n' % (b, b, b, e, e, e, c, c, c))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--classes', default=','.join(CLASSES))
    ap.add_argument('--variants', type=int, default=6)
    ap.add_argument('--only-variant', type=int, default=0)
    ap.add_argument('--workers', type=int, default=6)
    ap.add_argument('--work', type=int, default=WORK_W, help='working width (2048 final; 1024 for quick looks)')
    a = ap.parse_args()
    cl = a.classes.split(',')
    vs = [a.only_variant] if a.only_variant else range(1, a.variants + 1)
    jobs = [(c, v, a.work) for c in cl if c in LOOKS for v in vs]
    if os.name == 'nt':
        import ctypes
        ctypes.windll.kernel32.SetPriorityClass(ctypes.windll.kernel32.GetCurrentProcess(), C.BELOW_NORMAL)
    if 'asteroids' in cl:
        make_asteroids()
        print('asteroids: 6 cratered rock meshes + per-rock maps', flush=True)
    with ProcessPoolExecutor(a.workers) as ex:
        for c, v in ex.map(make_variant, jobs):
            print('planet.%s variant %d done' % (c, v), flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
