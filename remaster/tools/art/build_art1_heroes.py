"""Build the L7 hero hulls: BelowNormal priority, Dummy audio, glTF with LODs.

    python tools/art/build_art1_heroes.py [--keys ship.fed.akira,ship.kli.*] [--no-stage]

Stages the picks (stage_art1.py), then runs Blender headless on blender_hero_ships.py --gltf. Outputs land in
LocalAssets/ships/<key>/ and godot-hero-manifest.json (merged), all in the main checkout.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import _common as C


def main():
    keys = sys.argv[sys.argv.index('--keys') + 1] if '--keys' in sys.argv else None
    env = dict(os.environ, BOTF_LOCAL_ASSETS=C.LOCAL)
    if '--no-stage' not in sys.argv:
        cmd = [sys.executable, os.path.join(C.HERE, 'stage_art1.py')] + (['--keys', keys] if keys else [])
        if C.run_below_normal(cmd, env=env).returncode:
            return 1
    cmd = [C.BLENDER, '-b', '--factory-startup', '-noaudio', '--python',
           os.path.join(C.HERE, 'blender_art1_entry.py'), '--', '--gltf']
    if keys:
        cmd += ['--keys', keys]
    return C.run_below_normal(cmd, env=env).returncode


if __name__ == '__main__':
    sys.exit(main())
