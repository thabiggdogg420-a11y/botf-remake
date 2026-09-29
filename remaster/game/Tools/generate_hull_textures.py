"""Generates the tiling hull-plating textures for the procedural ships (original art, no third-party input).

    python generate_hull_textures.py [--out ../ArtSource/Generated/Ships] [--size 2048]

Writes two seamless tiles (the ships' UVs repeat them every max(4, L/14) metres):
  T_HullPanels.png  RGBA data, not colour:
                    R panel tone, G seam/cavity, B lit window cells, A roughness
  T_HullNormal.png  tangent-space normal from the same panel height field (grooves, raised plates, hatches)
ue_import_generated_ships.py imports them into /Game/Generated/Ships and builds M_ShipHull on them.
Deterministic (fixed seed).

Faction plating: every ArtSource/Gemini/<faction>-plating.(png|jpg) (a plating tile generated with Gemini for this
project) is converted to the same channel layout as T_HullPanels_<faction>.png and T_HullNormal_<faction>.png: tone
and height from its luminance, grooves as cavity, the window cells shared with the default tile. Tiles that do not wrap
cleanly are made seamless with a half-offset cross-fade. The importer gives that faction's material these textures.
"""
import glob
import argparse
import os

import numpy as np
from PIL import Image


def panel_ids(n, cell, rng):
    """Tileable rectangle packing on an n/cell grid: each panel covers 1-4 x 1-3 cells (wrapping)."""
    g = n // cell
    ids = -np.ones((g, g), np.int32)
    count = 0
    for y in range(g):
        for x in range(g):
            if ids[y, x] >= 0:
                continue
            w, h = rng.integers(1, 5), rng.integers(1, 4)
            for dy in range(h):
                for dx in range(w):
                    yy, xx = (y + dy) % g, (x + dx) % g
                    if ids[yy, xx] < 0:
                        ids[yy, xx] = count
            count += 1
    return np.kron(ids, np.ones((cell, cell), np.int32)), count


def value_noise(n, octaves, rng, base=8):
    out = np.zeros((n, n), np.float32)
    amp, total = 1.0, 0.0
    for o in range(octaves):
        k = base * 2 ** o
        grid = rng.random((k, k)).astype(np.float32)
        # Bilinear upsample with wrap so the tile stays seamless.
        t = np.arange(n, dtype=np.float32) * k / n
        i0 = np.floor(t).astype(int) % k
        i1 = (i0 + 1) % k
        f = t - np.floor(t)
        f = f * f * (3 - 2 * f)
        rows = grid[i0][:, i0] * (1 - f)[None, :] + grid[i0][:, i1] * f[None, :]
        rows1 = grid[i1][:, i0] * (1 - f)[None, :] + grid[i1][:, i1] * f[None, :]
        out += amp * (rows * (1 - f)[:, None] + rows1 * f[:, None])
        total += amp
        amp *= 0.5
    return out / total


def dilate(mask, r):
    out = mask.copy()
    for dy in range(-r, r + 1):
        for dx in range(-r, r + 1):
            out |= np.roll(np.roll(mask, dy, 0), dx, 1)
    return out


def seamless(img):
    """Cross-fade with a copy rolled by half a tile: the copy is continuous where the original has its edges."""
    n = img.shape[0]
    rolled = np.roll(np.roll(img, n // 2, 0), n // 2, 1)
    ramp = np.minimum(np.arange(n), np.arange(n)[::-1]).astype(np.float32) / (n * 0.25)
    w1 = np.clip(ramp, 0, 1)
    w = np.minimum(w1[:, None], w1[None, :])
    if img.ndim == 3:
        w = w[..., None]
    return img * w + rolled * (1 - w)


def box_blur(a, r):
    out = a.copy()
    for _ in range(2):
        acc = np.zeros_like(out)
        for d in range(-r, r + 1):
            acc += np.roll(out, d, 0)
        out = acc / (2 * r + 1)
        acc = np.zeros_like(out)
        for d in range(-r, r + 1):
            acc += np.roll(out, d, 1)
        out = acc / (2 * r + 1)
    return out


def write_normal(height, path, strength=6.0):
    blur = height
    for _ in range(2):
        blur = (blur + np.roll(blur, 1, 0) + np.roll(blur, -1, 0) + np.roll(blur, 1, 1) + np.roll(blur, -1, 1)) / 5
    dx = (np.roll(blur, -1, 1) - np.roll(blur, 1, 1)) * strength
    dy = (np.roll(blur, -1, 0) - np.roll(blur, 1, 0)) * strength
    nz = np.ones_like(dx)
    ln = np.sqrt(dx * dx + dy * dy + nz * nz)
    normal = np.stack([-dx / ln, dy / ln, nz / ln], -1)  # +Y up in UE tangent space (image rows grow downward)
    Image.fromarray(((normal * 0.5 + 0.5) * 255 + 0.5).astype(np.uint8), "RGB").save(path)


def faction_plating(src, n, win, out_dir, fac):
    lum = np.asarray(Image.open(src).convert("L").resize((n, n), Image.LANCZOS)).astype(np.float32) / 255.0
    lo, hi = np.percentile(lum, 2), np.percentile(lum, 98)
    lum = np.clip((lum - lo) / max(1e-3, hi - lo), 0, 1)
    edge = max(abs(lum[0] - lum[-1]).mean(), abs(lum[:, 0] - lum[:, -1]).mean())
    inner = min(abs(lum[1:] - lum[:-1]).mean(), abs(lum[:, 1:] - lum[:, :-1]).mean())
    if edge > inner * 1.3:
        lum = seamless(lum)
    local = box_blur(lum, max(2, n // 128))
    cavity = np.clip((local - lum) * 4.0, 0, 1)
    tone = np.clip(0.2 + lum * 0.6, 0, 1)
    # Rougher than the kit tile: a generated image carries photographic grain, which as a glossy normal map glitters.
    rough = np.clip(0.62 + (0.5 - lum) * 0.2 + cavity * 0.2, 0.05, 1)
    rgba = np.stack([tone, cavity, win, rough], -1)
    Image.fromarray((rgba * 255 + 0.5).astype(np.uint8), "RGBA").save(os.path.join(out_dir, "T_HullPanels_%s.png" % fac))
    # Height from the plate structure only: the grain is blurred out before the normal is taken.
    height = box_blur(lum, max(1, n // 512)) * 0.6 - box_blur(cavity, max(1, n // 1024)) * 0.6 - (win > 0) * 0.3
    write_normal(height, os.path.join(out_dir, "T_HullNormal_%s.png" % fac), 2.5)
    print("plating", fac, "from", os.path.basename(src), "(made seamless)" if edge > inner * 1.3 else "")


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(here, "..", "ArtSource", "Generated", "Ships"))
    ap.add_argument("--size", type=int, default=2048)
    a = ap.parse_args()
    n = a.size
    rng = np.random.default_rng(1999)
    s = n / 2048.0

    # Large plates, then a finer subdivision inside some of them.
    ids, count = panel_ids(n, int(64 * s), rng)
    fine, fcount = panel_ids(n, int(32 * s), rng)
    tone_of = rng.normal(0.5, 0.11, count).clip(0.15, 0.85).astype(np.float32)
    tone = tone_of[ids]
    fine_tone = rng.normal(0.0, 0.05, fcount).astype(np.float32)
    use_fine = (rng.random(count) < 0.35)[ids]
    tone = tone + np.where(use_fine, fine_tone[fine], 0.0)

    seam = (ids != np.roll(ids, 1, 0)) | (ids != np.roll(ids, 1, 1))
    seam = dilate(seam, max(1, int(round(1 * s))))
    fseam = ((fine != np.roll(fine, 1, 0)) | (fine != np.roll(fine, 1, 1))) & use_fine

    # Raised hatches and access plates (small rectangles), and recessed vents (thin slots).
    height = tone_of[ids] * 0.25
    hatch = np.zeros((n, n), bool)
    vent = np.zeros((n, n), bool)
    for _ in range(int(220 * s * s)):
        w, h = int(rng.integers(10, 44) * s), int(rng.integers(8, 30) * s)
        x, y = rng.integers(0, n), rng.integers(0, n)
        ys, xs = np.arange(y, y + h) % n, np.arange(x, x + w) % n
        hatch[np.ix_(ys, xs)] = True
    for _ in range(int(90 * s * s)):
        x, y = rng.integers(0, n), rng.integers(0, n)
        k = int(rng.integers(3, 8))
        for i in range(k):
            ys = np.arange(y + i * int(6 * s), y + i * int(6 * s) + max(1, int(2 * s))) % n
            xs = np.arange(x, x + int(rng.integers(20, 50) * s)) % n
            vent[np.ix_(ys, xs)] = True
    hatch_edge = hatch & ~(np.roll(hatch, 1, 0) & np.roll(hatch, -1, 0) & np.roll(hatch, 1, 1) & np.roll(hatch, -1, 1))
    height = height + hatch * 0.35 - seam * 0.9 - fseam * 0.35 - vent * 0.5

    # Window cells: 8 decks per tile, 24 windows a deck, some dark.
    win = np.zeros((n, n), np.float32)
    deck = n // 8
    pitch = n // 24
    ww, wh = int(pitch * 0.46), int(deck * 0.075)
    for d in range(8):
        cy = d * deck + deck // 2
        for i in range(24):
            if rng.random() < 0.22:
                continue
            cx = i * pitch + pitch // 2
            ys = np.arange(cy - wh // 2, cy + wh // 2 + 1) % n
            xs = np.arange(cx - ww // 2, cx + ww // 2 + 1) % n
            win[np.ix_(ys, xs)] = rng.uniform(0.6, 1.0)
    height = height - (win > 0) * 0.3

    grime = value_noise(n, 5, rng, base=4)
    tone = np.clip(tone * (0.85 + 0.3 * grime), 0, 1)
    cavity = np.clip(seam * 1.0 + fseam * 0.5 + vent * 0.7 + hatch_edge * 0.4, 0, 1)
    rough = np.clip(0.42 + (tone - 0.5) * 0.3 + (grime - 0.5) * 0.35 + cavity * 0.25 - hatch * 0.06, 0.05, 1)

    rgba = np.stack([tone, cavity, win, rough], -1)
    os.makedirs(a.out, exist_ok=True)
    Image.fromarray((rgba * 255 + 0.5).astype(np.uint8), "RGBA").save(os.path.join(a.out, "T_HullPanels.png"))

    # Normal from the height field (central differences, wrapped, so the tile stays seamless).
    write_normal(height, os.path.join(a.out, "T_HullNormal.png"))
    gemini = os.path.join(here, "..", "ArtSource", "Gemini")
    for src in sorted(glob.glob(os.path.join(gemini, "*-plating.*"))):
        fac = os.path.basename(src).split("-plating")[0]
        faction_plating(src, n, win, a.out, fac)
    print("wrote", os.path.abspath(a.out))


if __name__ == "__main__":
    main()
