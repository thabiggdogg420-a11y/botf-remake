"""Stage the L7 hero picks and write LocalAssets/art1/hero-ships-art1.json (private, gitignored).

    python tools/art/stage_art1.py [--check] [--keys ship.fed.akira,...]

Reuses game/Tools/stage_bc_heroes.py (unmodified) with its paths redirected to the main checkout's LocalAssets, so the
shared hero-ships.json and bc-picks.json are never touched. Lengths come from the asset manifest (canon length_m).
"""
import fnmatch
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import _common as C

sys.path.insert(0, os.path.join(C.REMASTER, 'game', 'Tools'))
import stage_bc_heroes as S

S.INCOMING = os.path.join(C.LOCAL, 'incoming', 'bc')
OUT = os.path.join(C.LOCAL, 'art1', 'hero-ships-art1.json')


def main():
    check = '--check' in sys.argv
    pats = None
    if '--keys' in sys.argv:
        pats = sys.argv[sys.argv.index('--keys') + 1].split(',')
    picks = C.load_json(os.path.join(C.HERE, 'art1_picks.json'))
    manifest = C.load_json(os.path.join(C.REMASTER, 'assets', 'asset-manifest.json'))
    length = {r['key']: r['length_m'] for r in manifest['assets'] if r['category'] == 'ship'}
    ships = []
    for p in picks['picks']:
        if pats and not any(fnmatch.fnmatch(p['key'], x) for x in pats):
            continue
        if 'glb' in p:  # repo-local community model, not a BC conversion
            ships.append({'key': p['key'], 'source': os.path.join(C.MAIN, p['glb']), 'forward': p.get('forward', '+X'),
                          'up': p.get('up', '+Z'), 'lengthMeters': length[p['key']], 'metallic': 0.55})
            continue
        pick = dict(p)
        pick['lengthMeters'] = length[p['key']]
        pick.setdefault('metallic', 0.45 if '.kli.' in p['key'] else 0.55)
        ships.append(S.stage(pick, picks['bcRoot'], picks['hdRoot'], check))
    if check:
        return
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    doc = {'_comment': 'L7 art wave 1 hero config, written by tools/art/stage_art1.py. Private.',
           'defaults': {'maxTriangles': 60000, 'metallic': 0.5}, 'ships': ships}
    with open(OUT, 'w', encoding='utf-8') as f:
        json.dump(doc, f, indent=1)
    print('%s: %d ships' % (OUT, len(ships)))


if __name__ == '__main__':
    main()
