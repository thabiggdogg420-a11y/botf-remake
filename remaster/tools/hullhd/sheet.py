"""Labelled contact sheet of hull_gallery frames.
  python sheet.py out.png <frames_dir> <ships_dir> key [key ...]   [--cols 5] [--w 640]
Each tile: hull-<key>.png, labelled with the class key, the base-colour texture size and HD / 1x (from the hero's
hero.json bcPx, else the BC png/jpg beside the glb)."""
import json, os, sys
from PIL import Image, ImageDraw
a = sys.argv[1:]
cols, tw = 5, 640
for flag, name in (("--cols", "cols"), ("--w", "tw")):
    if flag in a:
        i = a.index(flag); v = int(a[i + 1]); del a[i:i + 2]
        if name == "cols": cols = v
        else: tw = v
out, frames, ships, keys = a[0], a[1], a[2], a[3:]
th = tw * 9 // 16
def texinfo(key):
    d = os.path.join(ships, key)
    best = 0
    try:
        for s in json.load(open(os.path.join(d, "hero.json")))["slots"]:
            if s.get("bcPx"): best = max(best, *s["bcPx"])
    except Exception: pass
    if best:
        return "%d px HD" % best
    try:
        for f in os.listdir(d):
            if f.endswith("_BC.png") or f.endswith("_BC.jpg"):
                best = max(best, *Image.open(os.path.join(d, f)).size)
    except Exception: pass
    return ("%d px 1x" % best) if best else "no tex"
rows = (len(keys) + cols - 1) // cols
sheet = Image.new("RGB", (cols * tw, rows * (th + 18)), (8, 8, 12))
dr = ImageDraw.Draw(sheet)
for i, k in enumerate(keys):
    x, y = (i % cols) * tw, (i // cols) * (th + 18)
    p = os.path.join(frames, "hull-%s.png" % k)
    try:
        sheet.paste(Image.open(p).convert("RGB").resize((tw, th), Image.LANCZOS), (x, y + 18))
    except Exception:
        dr.text((x + 8, y + 40), "missing frame", fill=(220, 90, 90))
    dr.text((x + 5, y + 3), "%s   %s" % (k, texinfo(k)), fill=(255, 255, 255))
sheet.save(out)
print(out, sheet.size)
