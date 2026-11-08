"""Build the L9 ships: the kitbashed hulls and the 30 m Peregrine, then running lights, Federation cool-grey tints.

    python tools/art/build_art2_ships.py [--keys ship.fed.sydney,...] [--no-lights]

Blender runs at BelowNormal priority with -noaudio. Outputs land in the main checkout's LocalAssets/ships (never committed).
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import _common as C


def main():
    keys = sys.argv[sys.argv.index('--keys') + 1] if '--keys' in sys.argv else None
    env = dict(os.environ, BOTF_LOCAL_ASSETS=C.LOCAL)
    cmd = [C.BLENDER, '-b', '--factory-startup', '-noaudio', '--python', os.path.join(C.HERE, 'blender_art2_entry.py'), '--']
    if keys:
        cmd += ['--keys', keys]
    rc = C.run_below_normal(cmd, env=env).returncode
    return rc


if __name__ == '__main__':
    sys.exit(main())
