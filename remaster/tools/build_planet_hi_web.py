#!/usr/bin/env python3
"""Builds planets_hi_web.pck: the desktop planet tier for the web build (M1 webparity), so a big planet in the browser (the
battle backdrop, the system screen's large orb) uses the same 2048 px albedo, normal, roughness, cloud and night maps the
desktop draws, instead of the 1024 px web tier.

  python remaster/tools/build_planet_hi_web.py --planets <LocalAssets/planets> --out <web>/packs/planets_hi_web.pck
      [--stage <dir>] [--godot <console exe>] [--proj remaster/godot] [--quality 90]

The desktop caps every planet image at 2048 px wide when it loads it (PlanetTextures.MAX_WIDTH), so the 4096 albedo is
resized here with the same Lanczos filter and every layer is stored as WebP (normal maps at quality 96). Files land under
res://assets/planets/<class>/desktop/, where PlanetTextures.variants looks for the hi tier. The private textures stay out
of git; the pack is a build output. Rebuilt only when an input is newer.
"""
import argparse
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from build_ship_packs import newer, pack  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--planets", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--stage", default=None)
    ap.add_argument("--godot", default=os.environ.get("GODOT", r"C:\Tools\Godot\Godot_v4.5-stable_win64_console.exe"))
    ap.add_argument("--proj", default=os.path.normpath(os.path.join(HERE, "..", "godot")))
    ap.add_argument("--quality", type=int, default=90)
    ap.add_argument("--max", type=int, default=2048)
    a = ap.parse_args()
    from PIL import Image
    stage = os.path.abspath(a.stage or os.path.join(a.proj, "build", "planets-hi-stage"))
    os.makedirs(stage, exist_ok=True)
    open(os.path.join(stage, ".gdignore"), "w").close()
    entries, src_total, out_total = [], 0, 0
    for cls in sorted(os.listdir(a.planets)):
        d = os.path.join(a.planets, cls, "desktop")
        if not os.path.isdir(d):
            continue
        for f in sorted(os.listdir(d)):
            if not f.lower().endswith((".png", ".jpg", ".webp")):
                continue
            src = os.path.join(d, f)
            dst = os.path.join(stage, cls, os.path.splitext(f)[0] + ".webp")
            if newer(dst, [src, os.path.abspath(__file__)]):
                os.makedirs(os.path.dirname(dst), exist_ok=True)
                im = Image.open(src)
                im.load()
                if im.width > a.max:
                    im = im.resize((a.max, round(im.height * a.max / im.width)), Image.LANCZOS)
                if im.mode not in ("RGB", "RGBA", "L"):
                    im = im.convert("RGB")
                if im.mode == "L":
                    im = im.convert("RGB")   # WebP has no grey mode; the loader reads the red channel either way
                im.save(dst, "WEBP", quality=96 if "_normal" in f else a.quality, method=5)
            src_total += os.path.getsize(src)
            out_total += os.path.getsize(dst)
            entries.append(("res://assets/planets/%s/desktop/%s" % (cls, os.path.basename(dst)), dst))
    if not entries:
        sys.exit("no desktop planet tier under %s" % a.planets)
    if newer(a.out, [e[1] for e in entries] + [os.path.abspath(__file__)]):
        pack(a.godot, a.proj, entries, a.out)
    print("planets_hi_web: %d files, %.1f MB of sources -> %.1f MB pck" % (len(entries), src_total / 1048576.0, os.path.getsize(a.out) / 1048576.0))


if __name__ == "__main__":
    main()
