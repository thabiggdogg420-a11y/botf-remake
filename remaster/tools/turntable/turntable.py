#!/usr/bin/env python3
"""Render asset keys to 8-angle contact sheets with Godot (Dummy audio).

  python remaster/tools/turntable/turntable.py ship.fed.galaxy planet.m
  python remaster/tools/turntable/turntable.py --renderer compat ship.kli.vorcha     web look (gl_compatibility, OpenGL)
  python remaster/tools/turntable/turntable.py --renderer both --out C:/tmp/tt ship.fed.defiant
  python remaster/tools/turntable/turntable.py --category ship --faction fed --milestone M1   keys from the manifest

--renderer desktop (default, Forward+ / Vulkan), compat (Compatibility / OpenGL3, the web renderer), or both.
Output: <out>/<renderer>/<key>.sheet.png and .json. Default out: remaster/captures/turntable (gitignored).
Godot: $GODOT, else C:\\Tools\\Godot\\Godot_v4.5-stable_win64_console.exe. Hero models resolve through $BOTF_HERO_MANIFEST
or the usual LocalAssets search (BattleAssets); with none, ships fall back to the procedural hulls. Opens a window
for the capture (needs the GPU); nothing is committed.
"""
import argparse
import json
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REMASTER = os.path.abspath(os.path.join(HERE, '..', '..'))
PROJECT = os.path.join(REMASTER, 'godot')
MANIFEST = os.path.join(REMASTER, 'assets', 'asset-manifest.json')
SCRIPT = os.path.join(HERE, 'turntable.gd')
DEFAULT_GODOT = r'C:\Tools\Godot\Godot_v4.5-stable_win64_console.exe'
RENDERERS = {
    'desktop': ['--rendering-method', 'forward_plus'],
    'compat': ['--rendering-method', 'gl_compatibility', '--rendering-driver', 'opengl3'],
}


def planets_arg():
    """Point the planet mode at LocalAssets/planets (main checkout; $BOTF_LOCAL_ASSETS overrides) when it exists."""
    main = os.environ.get('BOTF_MAIN_REMASTER', r'C:\Users\BiggDogg\Documents\ChatGPT\Game Remakes\remaster')
    d = os.path.join(os.environ.get('BOTF_LOCAL_ASSETS') or os.path.join(main, 'game', 'LocalAssets'), 'planets')
    return ['--planets=' + d.replace('\\', '/')] if os.path.isdir(d) else []


def keys_from_manifest(a):
    doc = json.load(open(MANIFEST, encoding='utf8'))
    out = []
    for r in doc['assets']:
        if not r['key'].startswith(('ship.', 'planet.')) or r.get('planned'):
            continue
        if a.category and r['category'] != a.category:
            continue
        if a.faction and r.get('faction') != a.faction:
            continue
        if a.milestone and r['milestone'] != a.milestone:
            continue
        out.append(r['key'])
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('keys', nargs='*')
    ap.add_argument('--renderer', choices=['desktop', 'compat', 'both'], default='desktop')
    ap.add_argument('--out', default=os.path.join(REMASTER, 'captures', 'turntable'))
    ap.add_argument('--size', default='384')
    ap.add_argument('--elev', default='14')
    ap.add_argument('--rig', choices=['show', 'bible'], default='show', help='show: hard key, near-zero ambient (benchmark); bible: section 4 rig')
    ap.add_argument('--variant', default='1', help='planet texture variant 1-6')
    ap.add_argument('--category')
    ap.add_argument('--faction')
    ap.add_argument('--milestone')
    a = ap.parse_args()

    keys = a.keys or (keys_from_manifest(a) if (a.category or a.faction or a.milestone) else [])
    if not keys:
        ap.error('give keys, or --category/--faction/--milestone to pick them from the manifest')
    godot = os.environ.get('GODOT', DEFAULT_GODOT)
    modes = ['desktop', 'compat'] if a.renderer == 'both' else [a.renderer]
    rc = 0
    for mode in modes:
        out = os.path.join(a.out, mode)
        os.makedirs(out, exist_ok=True)
        cmd = [godot, '--path', PROJECT, '--audio-driver', 'Dummy', '--resolution', '480x270'] + RENDERERS[mode] + [
            '-s', SCRIPT, '--', '--keys=' + ','.join(keys), '--out=' + out, '--size=' + a.size, '--elev=' + a.elev, '--rig=' + a.rig, '--variant=' + a.variant] + planets_arg()
        print('turntable [%s]: %d key(s) -> %s' % (mode, len(keys), out))
        p = subprocess.run(cmd, cwd=PROJECT, creationflags=0x4000 if os.name == 'nt' else 0)
        rc = rc or p.returncode
    return rc


if __name__ == '__main__':
    sys.exit(main())
