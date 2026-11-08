"""Set bcTint on the Klingon picks so every hull's base colour sits in the Klingon olive family (ART-BIBLE section 3,
hull base (0.33,0.36,0.30); Dominion War footage reads dark olive-grey, not pale or blue).

    python tools/art/klingon_tints.py [--write] [key ...]

Measures the area-weighted mean of each staged base-colour texture (LocalAssets/incoming/bc/<id>/textures), then prints
tint = target / mean, clipped to 0.45..1.15 per channel. With --write the values go into art1_picks.json (bcTint).
Hulls already inside 12 percent of the target are left untinted.
"""
import json
import os
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import _common as C

TARGET = np.array([0.33, 0.36, 0.30])


def mean_colour(bc_id):
    root = os.path.join(C.LOCAL, 'incoming', 'bc', bc_id, 'textures')
    tot, n = np.zeros(3), 0
    for f in os.listdir(root):
        if f.endswith('_basecolor.png') or f.endswith('_basecolor_4x.png'):
            a = np.asarray(Image.open(os.path.join(root, f)).convert('RGB').resize((128, 128)), dtype=float) / 255
            # ignore near-white and near-black pixels (lights, decals) so the paint drives the estimate
            lum = a.mean(-1)
            m = (lum > 0.08) & (lum < 0.85)
            if m.any():
                tot += a[m].sum(0)
                n += m.sum()
    return tot / max(n, 1)


def main():
    picks_path = os.path.join(C.HERE, 'art1_picks.json')
    doc = C.load_json(picks_path)
    only = [a for a in sys.argv[1:] if not a.startswith('--')]
    for p in doc['picks']:
        if '.kli.' not in p['key'] or (only and p['key'] not in only):
            continue
        m = mean_colour(p['bc'])
        t = np.clip(TARGET / np.maximum(m, 1e-3), 0.45, 1.15)
        close = np.all(np.abs(m - TARGET) / TARGET < 0.12)
        print('%-20s mean %s tint %s%s' % (p['key'], m.round(3), t.round(2), ' (already olive)' if close else ''))
        if '--write' in sys.argv and not close and p['key'] != 'ship.kli.vorcha':
            p['bcTint'] = [round(float(x), 3) for x in t]
    if '--write' in sys.argv:
        with open(picks_path, 'w') as f:
            json.dump(doc, f, indent=1)


if __name__ == '__main__':
    main()
