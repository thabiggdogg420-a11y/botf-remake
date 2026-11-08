"""Write the L9 (art wave 2) results into assets/asset-manifest.json: status and the fields that describe the result only.

    python tools/art/mark_manifest_art2.py [--hold key=reason ...] [--planet-hold class=reason ...]

- the 14 kitbashed hulls and the Peregrine: has_hero_model, hero_lod0_tris, source, review_note; needs_model removed; status
  final-candidate (placeholder for --hold keys, with the reason as review_note).
- every hero hull with running lights: running_lights = the light node names; Federation hulls get a cool-grey note.
- planet class rows and the cloud / night-light layers: source rewritten for the L9 sets; status final-candidate, or
  placeholder for --planet-hold classes.
final-candidate means built and looked at on the desktop and Compatibility turntable sheets; the orchestrator decides final.
"""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import _common as C

MAN = os.path.join(C.REMASTER, 'assets', 'asset-manifest.json')
NEW = ['ship.fed.edward', 'ship.fed.london', 'ship.fed.sydney', 'ship.fed.soyuz', 'ship.fed.vengeance', 'ship.fed.wells',
       'ship.fed.yclass', 'ship.fed.k7', 'ship.kli.charghwi', 'ship.kli.d12', 'ship.kli.liwi', 'ship.kli.mayduj',
       'ship.kli.neghtev', 'ship.kli.qang', 'ship.fed.peregrine']
CLASSES = ['m', 'o', 'p', 'l', 'h', 'k', 'y', 'gasgiant', 'asteroids']


def holds(flag):
    out = {}
    argv = sys.argv[1:]
    for i, a in enumerate(argv):
        if a == flag and i + 1 < len(argv) and '=' in argv[i + 1]:
            k, v = argv[i + 1].split('=', 1)
            out[k] = v
    return out


def main():
    hold, phold = holds('--hold'), holds('--planet-hold')
    doc = C.load_json(MAN)
    by = {r['key']: r for r in doc['assets']}
    heroes = C.load_json(os.path.join(C.LOCAL, 'ships', 'godot-hero-manifest.json'))['heroes']
    ship_dir = os.path.join(C.LOCAL, 'ships')
    for k in NEW:
        r = by[k]
        hj = C.load_json(os.path.join(ship_dir, k, 'hero.json'))
        r['has_hero_model'] = True
        r['hero_lod0_tris'] = hj['triangles']
        r['source'] = ('hero glb (L9 art wave 2): %s; private' % hj['source'])
        r.pop('needs_model', None)
        r['status'] = 'placeholder' if k in hold else 'final-candidate'
        r['review_note'] = hold.get(k) or ('kitbash of %s, canon length %s m' % (hj['source'].split('kitbash of ')[-1].split(' (')[0], r['length_m'])
                                           if 'kitbash' in hj['source'] else 'original 30 m wedge fighter with twin nacelles; replaces the wrong-class 436 m BC saucer')
    n_lights = 0
    for k, e in heroes.items():
        if k in by and e.get('lights'):
            by[k]['running_lights'] = [l['node'] for l in e['lights']]
            n_lights += 1
        if k.startswith('ship.fed.') and k in by:
            by[k]['hull_tint_note'] = 'cool light grey (fed_tints.py tint in godot-hero-manifest.json), not cream'
    for c in CLASSES:
        k = 'planet.' + c
        if k not in by or not os.path.isdir(os.path.join(C.LOCAL, 'planets', c)):
            continue
        r = by[k]
        r['status'] = 'placeholder' if c in phold else 'final-candidate'
        if c in phold:
            r['review_note'] = phold[c]
        else:
            r.pop('review_note', None)
        r['source'] = ('procedural, original work (L9 rebuild, tools/art/gen_planets.py + planet_noise.py): LocalAssets/planets/%s; ' % c +
                       ('6 cratered rock meshes (S/C/M-type, real crater size distribution) with per-rock albedo and normal' if c == 'asteroids' else
                        'fBm / ridged heightfields, latitude-altitude-moisture biomes, power-law craters or zonal-jet advection (gas giant); '
                        '6 desktop variants (albedo 4096x2048, normal, roughness, clouds, night/lava), 4 web variants (1024x512 webp, relief baked, 512x256 layers)'))
    for k in ('planet.layer.clouds', 'planet.layer.night_lights'):
        if k in by:
            by[k]['status'] = 'final-candidate'
            by[k]['source'] = 'procedural layers generated with each class set (gen_planets.py): cover 26 to 40 percent so the surface stays readable; night lights, and planet Y lava emission (kept on in Compatibility)'
    with open(MAN, 'w', encoding='utf8') as f:
        json.dump(doc, f, indent=2, ensure_ascii=False)
        f.write('\n')
    print('kitbash rows', len(NEW), 'lights rows', n_lights)


if __name__ == '__main__':
    main()
