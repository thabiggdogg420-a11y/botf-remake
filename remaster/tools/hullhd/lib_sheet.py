"""Contact sheet of library review renders: python lib_sheet.py out.png view Id Id ...   (view = iso|side|top)
Reads H:/StarhelmTools/bc-ships/<Id>/review/<view>.png (read only), labels each with the Id, LOD0 tris and max texture."""
import json, os, sys
from PIL import Image, ImageDraw
ROOT = "H:/StarhelmTools/bc-ships"
out, view, ids = sys.argv[1], sys.argv[2], sys.argv[3:]
cols = 5
cell = 400
rows = (len(ids) + cols - 1) // cols
sheet = Image.new("RGB", (cols * cell, rows * (cell + 34)), (12, 12, 16))
d = ImageDraw.Draw(sheet)
for i, bid in enumerate(ids):
    x, y = (i % cols) * cell, (i // cols) * (cell + 34)
    p = f"{ROOT}/{bid}/review/{view}.png"
    try:
        im = Image.open(p).convert("RGB").resize((cell, cell))
        sheet.paste(im, (x, y + 34))
    except Exception as e:
        d.text((x + 10, y + 60), "no review image", fill=(200, 80, 80))
    tris = tex = "?"
    try:
        s = json.load(open(f"{ROOT}/{bid}/ship.json"))
        tris = s["lods"][0]["triangles"]
    except Exception: pass
    try:
        mx = 0
        for f in os.listdir(f"{ROOT}/{bid}/textures"):
            if f.endswith("_basecolor.png"):
                w, h = Image.open(f"{ROOT}/{bid}/textures/{f}").size
                mx = max(mx, w, h)
        tex = mx
    except Exception: pass
    d.text((x + 6, y + 6), f"{bid}", fill=(255, 255, 255))
    d.text((x + 6, y + 19), f"tris {tris}  tex {tex}", fill=(150, 220, 150))
sheet.save(out)
print(out, sheet.size)
