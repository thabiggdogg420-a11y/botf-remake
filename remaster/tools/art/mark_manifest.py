"""Write the L7 results into assets/asset-manifest.json (status and the fields that describe the result only).

    python tools/art/mark_manifest.py [--hold ship.fed.peregrine=reason ...]

- ship rows for every art1 pick: status final-candidate (placeholder for --hold keys), has_hero_model, hero_lod0_tris,
  source, review_note; rows in art1_picks.json needs_model get a needs_model object and stay procedural.
- planet rows (classes, clouds and night-lights layers): final-candidate when LocalAssets/planets/<class> exists.
- race portrait rows: placeholder plus original_extracted/local_file, from LocalAssets/portraits/results.json.
final-candidate means built and looked at; it is not final until the orchestrator reviews it (ART-BIBLE section 11).
"""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import _common as C

MAN = os.path.join(C.REMASTER, 'assets', 'asset-manifest.json')
NOTES = {
    'ship.kli.vorcha': 'tinted from teal-grey to Klingon olive (bcTint 0.55,0.5,0.4); darker for the Dominion War look',
    'ship.fed.spacedock': 'BC drydock is an open lattice frame (800 m, scaled to 1800 m); reads see-through from some angles',
    'ship.fed.ranger': 'in-repo Icewolf132 model (permission clear), 677 tris: low detail',
    'ship.fed.saber': '703 tris: the only Saber in the library, low detail',
    'ship.fed.kelvin_type': 'bckelvin_refit: a Kelvin-era hull, not confirmed to be the USS Kelvin; orchestrator to confirm',
    'ship.fed.constitution_kelvin': 'WCJJEnterprise (2009-film Enterprise)',
}


def main():
    doc = C.load_json(MAN)
    holds = dict(a[len('--hold'):] for a in []) if False else {}
    argv = sys.argv[1:]
    for a in argv:
        if '=' in a and not a.startswith('--'):
            k, v = a.split('=', 1)
            holds[k] = v
    if 'final-candidate' not in doc['statusValues']:
        doc['statusValues']['final-candidate'] = ('built and looked at by the art lane on both renderers; not final until the '
                                                  'orchestrator reviews it (ART-BIBLE section 11)')
    picks = C.load_json(os.path.join(C.HERE, 'art1_picks.json'))
    by = {r['key']: r for r in doc['assets']}
    ship_dir = os.path.join(C.LOCAL, 'ships')
    n = {'ship': 0, 'needs': 0, 'planet': 0, 'portrait': 0}
    for p in picks['picks']:
        r = by[p['key']]
        hj = C.load_json(os.path.join(ship_dir, p['key'], 'hero.json'))
        r['has_hero_model'] = True
        r['hero_lod0_tris'] = hj['triangles']
        r['source'] = ('hero glb (L7 art wave 1): %s; Blender blender_hero_ships.py --gltf, LODs, true length %s m; private'
                       % ('Bridge Commander model ' + p['bc'] + ' (Starhelm library)' if 'bc' in p else 'in-repo community model', r['length_m']))
        r['status'] = 'placeholder' if p['key'] in holds else 'final-candidate'
        note = holds.get(p['key']) or NOTES.get(p['key'])
        if note:
            r['review_note'] = note
        n['ship'] += 1
    for k, (why, base) in picks['needs_model'].items():
        by[k]['needs_model'] = {'reason': why, 'nearest_base_to_kitbash': base}
        n['needs'] += 1
    for k, r in by.items():
        if r['category'] == 'planet' and (k.startswith('planet.') and k.split('.')[1] in ('m', 'o', 'p', 'l', 'h', 'k', 'y', 'gasgiant', 'asteroids')):
            cls = k.split('.')[1]
            d = os.path.join(C.LOCAL, 'planets', cls)
            if os.path.isdir(d):
                r['status'] = 'final-candidate'
                r['source'] = ('procedural texture set (original work): LocalAssets/planets/%s, tools/art/gen_planets.py; '
                               % cls + ('6 rock meshes + rock albedo/normal' if cls == 'asteroids' else
                                        '6 desktop variants (albedo 4096x2048, normal, clouds, night where it fits), 4 web variants (1024x512 webp)'))
                n['planet'] += 1
    for k in ('planet.layer.clouds', 'planet.layer.night_lights'):
        if k in by:
            by[k]['status'] = 'final-candidate'
            by[k]['source'] = 'procedural layers generated with each class set: LocalAssets/planets/<class>/desktop/v<n>_clouds.png and _night.png (tools/art/gen_planets.py)'
    res = C.load_json(os.path.join(C.LOCAL, 'portraits', 'results.json'))
    for k, v in res.items():
        r = by[k]
        r['original_extracted'] = bool(v['found'])
        if v['found']:
            r['local_file'] = 'LocalAssets/portraits/%s.png' % k
            r['status'] = 'placeholder'
            r['source'] = ('original BOTF %s (stbof.res from the archive.org copy), extracted to LocalAssets/portraits, %dx%d; '
                           'private, reference or stand-in only (ART-BIBLE section 9)' % (v['file'], v['size'][0], v['size'][1]))
            n['portrait'] += 1
    with open(MAN, 'w', encoding='utf8') as f:
        json.dump(doc, f, indent=2, ensure_ascii=False)
        f.write('\n')
    print(n)


if __name__ == '__main__':
    main()
