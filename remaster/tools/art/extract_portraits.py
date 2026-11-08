"""Convert the original BOTF art (stbof.res, from the archive.org copy) to PNG and collect the race portraits.

    python tools/art/extract_portraits.py [--res-dir <dir with the unzipped stbof.res files>]

Input: LocalAssets/botf-original/stbof/files (stbof.res from archive.org `birthof-fed`, a zip with a hex-dumped copy
alongside; see HANDOFF "Original BOTF audio" for the item names). Output, all under LocalAssets (gitignored, private):
  botf-original/png/<name>.png          every TGA in stbof.res (UI art, race images, ministers, buildings)
  portraits/<manifest key>.png          the 35 race images at their native 160x170
  portraits/contact-sheet-128.png       all 35 at 128 px, for the row-alignment review (ART-BIBLE section 11)
  portraits/results.json                key -> {file, found, size}; tools/art/mark_manifest.py reads it
"""
import json
import os
import re
import sys

from PIL import Image, ImageDraw

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import _common as C

SRC = os.path.join(C.LOCAL, 'botf-original', 'stbof', 'files')
if '--res-dir' in sys.argv:
    SRC = sys.argv[sys.argv.index('--res-dir') + 1]
PNG = os.path.join(C.LOCAL, 'botf-original', 'png')
OUT = os.path.join(C.LOCAL, 'portraits')


def main():
    os.makedirs(PNG, exist_ok=True)
    os.makedirs(OUT, exist_ok=True)
    n = 0
    for f in sorted(os.listdir(SRC)):
        if f.lower().endswith('.tga'):
            dst = os.path.join(PNG, f[:-4] + '.png')
            if not os.path.exists(dst):
                try:
                    Image.open(os.path.join(SRC, f)).save(dst)
                    n += 1
                except Exception as e:
                    print('skip', f, e)
    print('converted', n, 'tga to png')
    man = C.load_json(os.path.join(C.REMASTER, 'assets', 'asset-manifest.json'))
    res = {}
    tiles = []
    for r in man['assets']:
        if not r['key'].startswith('portrait.') or r.get('planned'):
            continue
        m = re.search(r'original (\S+\.tga)', r['source'])
        if not m:
            continue
        name = m.group(1)
        p = os.path.join(SRC, name)
        if not os.path.isfile(p):
            res[r['key']] = {'file': name, 'found': False}
            continue
        im = Image.open(p).convert('RGBA')
        im.save(os.path.join(OUT, r['key'] + '.png'))
        res[r['key']] = {'file': name, 'found': True, 'size': list(im.size)}
        tiles.append((r['key'], im))
    cols = 7
    cw, ch = 128, 136
    rows = (len(tiles) + cols - 1) // cols
    sheet = Image.new('RGB', (cols * cw, rows * (ch + 14)), (10, 16, 34))
    d = ImageDraw.Draw(sheet)
    for i, (k, im) in enumerate(tiles):
        x, y = (i % cols) * cw, (i // cols) * (ch + 14)
        t = im.resize((cw, ch), Image.LANCZOS)
        sheet.paste(t, (x, y), t)
        d.text((x + 2, y + ch), k.replace('portrait.', ''), fill=(200, 210, 230))
    sheet.save(os.path.join(OUT, 'contact-sheet-128.png'))
    with open(os.path.join(OUT, 'results.json'), 'w') as f:
        json.dump(res, f, indent=1)
    print('portraits: %d found of %d' % (sum(1 for v in res.values() if v['found']), len(res)))


if __name__ == '__main__':
    main()
