"""Side-by-side before|after PNG: python pair.py out.png before.png after.png [label_before label_after] [--crop x0,y0,x1,y1]"""
import sys
from PIL import Image, ImageDraw
a = sys.argv[1:]
crop = None
if "--crop" in a:
    i = a.index("--crop"); crop = tuple(int(v) for v in a[i + 1].split(",")); del a[i:i + 2]
out, b, c = a[0], a[1], a[2]
lb = a[3] if len(a) > 3 else "before"
lc = a[4] if len(a) > 4 else "after"
ims = [Image.open(b).convert("RGB"), Image.open(c).convert("RGB")]
if crop:
    ims = [im.crop(crop) for im in ims]
w, h = ims[0].size
sheet = Image.new("RGB", (w * 2 + 6, h + 24), (10, 10, 14))
sheet.paste(ims[0], (0, 24)); sheet.paste(ims[1], (w + 6, 24))
d = ImageDraw.Draw(sheet)
d.text((8, 6), lb, fill=(255, 220, 140)); d.text((w + 14, 6), lc, fill=(140, 255, 160))
sheet.save(out)
print(out, sheet.size)
