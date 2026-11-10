#!/usr/bin/env python3
"""Side-by-side desktop/web pairs for the parity capture: pair-<shot>.png = desktop ui-<shot>.png (left) beside the web
frame (right), each 1280 px wide, labelled. The 3D-only shots (battle-volley, battle-closeup) use the web build's own
read-back (web-<shot>-internal.png) because the desktop saves the 3D view without its panels.
  python parity_pairs.py <desktop_dir> <web_dir> <out_dir> [shot ...]"""
import os
import sys
from PIL import Image, ImageDraw

INTERNAL = {"battle-volley", "battle-closeup", "battle-volley-close", "battle-campaign"}


def main():
    dd, wd, out = sys.argv[1:4]
    shots = sys.argv[4:] or ["title", "galaxy", "system", "battle-plan", "battle-volley", "battle-closeup"]
    os.makedirs(out, exist_ok=True)
    for s in shots:
        a = os.path.join(dd, "ui-%s.png" % s)
        b = os.path.join(wd, "web-%s%s.png" % (s, "-internal" if s in INTERNAL else ""))
        if not (os.path.isfile(a) and os.path.isfile(b)):
            print("missing", s)
            continue
        ia, ib = Image.open(a).convert("RGB"), Image.open(b).convert("RGB")
        w = 1280
        ia = ia.resize((w, int(ia.height * w / ia.width)), Image.LANCZOS)
        ib = ib.resize((w, int(ib.height * w / ib.width)), Image.LANCZOS)
        im = Image.new("RGB", (w * 2 + 8, max(ia.height, ib.height) + 28), (20, 20, 20))
        im.paste(ia, (0, 28))
        im.paste(ib, (w + 8, 28))
        d = ImageDraw.Draw(im)
        d.text((8, 8), "DESKTOP  %s" % s, fill=(255, 220, 120))
        d.text((w + 16, 8), "WEB  %s" % s, fill=(120, 200, 255))
        p = os.path.join(out, "pair-%s.png" % s)
        im.save(p)
        print(p)


if __name__ == "__main__":
    main()
