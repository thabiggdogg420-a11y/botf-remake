#!/usr/bin/env python3
"""Builds contact sheets from a verify run's capture folder.
  python make_sheets.py <run_dir>
Every group folder (screens, battles, galaxy, compat, web, ...) with PNGs gets sheet-<group>.png: a labelled grid of
thumbnails, 3 columns, each thumb 640 wide. Also writes sheets.json (group -> sheet path, shots, sizes, blank flags).
A shot is flagged "blank" when almost every pixel is one colour (the usual sign of a failed capture)."""
import json
import os
import sys

from PIL import Image, ImageDraw, ImageFont, ImageStat

COLS, TW = 3, 640


def blank(im):
    small = im.convert("L").resize((64, 36))
    return ImageStat.Stat(small).stddev[0] < 2.0


def main():
    run = sys.argv[1]
    try:
        font = ImageFont.truetype("arial.ttf", 20)
    except OSError:
        font = ImageFont.load_default()
    out = {}
    for group in sorted(d for d in os.listdir(run) if os.path.isdir(os.path.join(run, d))):
        gdir = os.path.join(run, group)
        files = sorted(f for f in os.listdir(gdir) if f.lower().endswith(".png"))
        if not files:
            continue
        thumbs, info = [], []
        for f in files:
            im = Image.open(os.path.join(gdir, f)).convert("RGB")
            th = max(1, round(TW * im.height / im.width))
            flagged = blank(im)
            info.append({"file": f, "size": [im.width, im.height], "blank": bool(flagged)})
            thumbs.append((f, im.resize((TW, th), Image.LANCZOS), flagged))
        rows = (len(thumbs) + COLS - 1) // COLS
        rh = [max(t[1].height for t in thumbs[r * COLS:(r + 1) * COLS]) + 30 for r in range(rows)]
        sheet = Image.new("RGB", (COLS * (TW + 8) + 8, sum(rh) + 8 * (rows + 1)), (14, 16, 26))
        d = ImageDraw.Draw(sheet)
        y = 8
        for r in range(rows):
            for c, (name, th, flagged) in enumerate(thumbs[r * COLS:(r + 1) * COLS]):
                x = 8 + c * (TW + 8)
                sheet.paste(th, (x, y + 30))
                label = os.path.splitext(name)[0] + ("  [BLANK]" if flagged else "")
                d.text((x + 4, y + 4), label, fill=(255, 80, 80) if flagged else (255, 170, 60), font=font)
            y += rh[r] + 8
        path = os.path.join(run, "sheet-%s.png" % group)
        sheet.save(path)
        out[group] = {"sheet": path, "shots": info}
        print("sheet %s: %d shots%s" % (group, len(files), "".join("  BLANK:" + i["file"] for i in info if i["blank"])))
    json.dump(out, open(os.path.join(run, "sheets.json"), "w"), indent=1)


if __name__ == "__main__":
    main()
