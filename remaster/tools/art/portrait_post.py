"""The one grade that makes the portraits a set: a navy-teal shadow tint, gentle contrast, a vignette that darkens the corners
into the LCARS panel colour, and fine grain. Run on every chosen portrait, majors and minors alike.

    python tools/art/portrait_post.py <in.png> <out.png>      (or --dir <in dir> --out <out dir>, keys <key>.png from <key>-s<seed>.png picks)
"""
import argparse, os
import numpy as np
from PIL import Image

def grade(im, seed=7):
    a = np.asarray(im.convert("RGB"), dtype=np.float32) / 255.0
    h, w, _ = a.shape
    lum = (a * np.array([0.299, 0.587, 0.114], dtype=np.float32)).sum(axis=2, keepdims=True)
    shadow = np.clip(1.0 - lum * 2.2, 0.0, 1.0)
    a = a + shadow * np.array([-0.010, 0.012, 0.030], dtype=np.float32)       # shadows toward navy-teal
    a = np.clip((a - 0.5) * 1.06 + 0.5, 0.0, 1.0)                             # gentle contrast
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    d = np.sqrt(((xx - w / 2) / (w / 2)) ** 2 + ((yy - h * 0.42) / (h * 0.62)) ** 2)
    v = 1.0 - np.clip(d - 0.75, 0.0, 1.0) ** 1.6 * 0.55
    a = a * v[..., None] + (1.0 - v[..., None]) * np.array([0.02, 0.04, 0.09], dtype=np.float32) * 0.6
    a = a + np.random.default_rng(seed).normal(0.0, 0.006, a.shape).astype(np.float32)
    return Image.fromarray((np.clip(a, 0, 1) * 255 + 0.5).astype(np.uint8))

if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("src", nargs="?"); ap.add_argument("dst", nargs="?")
    a = ap.parse_args()
    grade(Image.open(a.src)).save(a.dst)
