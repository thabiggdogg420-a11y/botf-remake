"""Cool-grey hulls for the Federation (L9): the BC paints of the library hulls read cream or warm white, the shows' Federation
hulls (TNG to DS9 Dominion War lighting, Picard S3) read cool light grey with a faint blue cast.

    python tools/art/fed_tints.py [--check] [key ...]

For every ship.fed.* hero, each paint slot whose mean colour is low-saturation (hull paint, not engine blue or window
red) gets a linear Tint in LocalAssets/ships/godot-hero-manifest.json (slots[].tint, the Tint hero_hull.gdshader
multiplies into BC) that moves its mean to a cool grey of the same luminance, clamped to 0.35..0.66 display luminance and
0.6..1.3 per channel so dark hulls (Defiant) stay dark and nothing flips colour family. art2_orig_tint keeps the first
value, so reruns are stable. Prints the before/after mean per slot.
"""
import json
import os
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import _common as C

SHIPS = os.path.join(C.LOCAL, 'ships')
MAN = os.path.join(SHIPS, 'godot-hero-manifest.json')
CAST = np.array([0.95, 1.0, 1.07])  # cool cast relative to neutral grey (display space)


def lin(a):
    return np.where(a <= 0.04045, a / 12.92, ((a + 0.055) / 1.055) ** 2.4)


def slot_mean(path):
    a = np.asarray(Image.open(path).convert('RGB').resize((192, 192)), np.float64) / 255
    lum = a.mean(-1)
    m = (lum > 0.08) & (lum < 0.92)
    if m.sum() < 50:
        return None
    return a[m].mean(0)


def main():
    check = '--check' in sys.argv
    only = [a for a in sys.argv[1:] if not a.startswith('--')]
    man = C.load_json(MAN)
    for key, e in sorted(man['heroes'].items()):
        if not key.startswith('ship.fed.') or (only and key not in only):
            continue
        hj = C.load_json(os.path.join(SHIPS, key, 'hero.json'))
        bc = {s['name']: s.get('bc') for s in hj['slots']}
        for s in e.get('slots', []):
            f = bc.get(s['name'])
            if not f or not os.path.exists(os.path.join(SHIPS, key, f)):
                continue
            if 'art2_orig_tint' not in s:
                s['art2_orig_tint'] = s.get('tint') or [1.0, 1.0, 1.0]
            mean = slot_mean(os.path.join(SHIPS, key, f))
            if mean is None:
                continue
            sat = (mean.max() - mean.min()) / max(mean.max(), 1e-6)
            L = float(mean @ np.array([0.2126, 0.7152, 0.0722]))
            if sat > 0.3 or L < 0.2:
                s['tint'] = list(s['art2_orig_tint'])
                print('%-26s %s keep   mean %s sat %.2f' % (key, s['name'], mean.round(2), sat))
                continue
            Lt = float(np.clip(L, 0.35, 0.66))
            tgt = CAST * Lt / (CAST @ np.array([0.2126, 0.7152, 0.0722]))
            t = np.clip(lin(tgt) / np.maximum(lin(mean), 1e-4), 0.6, 1.3)
            s['tint'] = [round(float(x), 3) for x in t]
            print('%-26s %s tint %s  mean %s -> %s' % (key, s['name'], s['tint'], mean.round(2), np.clip(mean * (t ** (1 / 2.2)), 0, 1).round(2)))
    if not check:
        with open(MAN, 'w', encoding='utf-8') as f:
            json.dump(man, f, indent=1)


if __name__ == '__main__':
    main()
