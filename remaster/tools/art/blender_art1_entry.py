"""Blender entry for build_art1_heroes.py: runs game/Tools/blender_hero_ships.py unmodified, with its CONFIG, INCOMING and
OUT_ROOT redirected to the main checkout's LocalAssets (the worktree has none). Args after '--' pass through."""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REMASTER = os.path.abspath(os.path.join(HERE, '..', '..'))
TOOLS = os.path.join(REMASTER, 'game', 'Tools')
LOCAL = os.environ['BOTF_LOCAL_ASSETS']
path = os.path.join(TOOLS, 'blender_hero_ships.py')
src = open(path, encoding='utf-8').read()
subs = [('CONFIG = os.path.join(GAME, "ArtSource", "HeroShips", "hero-ships.json")',
         'CONFIG = %r' % os.path.join(LOCAL, 'art1', 'hero-ships-art1.json')),
        ('INCOMING = os.path.join(GAME, "LocalAssets", "incoming")', 'INCOMING = %r' % os.path.join(LOCAL, 'incoming')),
        ('OUT_ROOT = os.path.join(GAME, "LocalAssets", "ships")', 'OUT_ROOT = %r' % os.path.join(LOCAL, 'ships'))]
for a, b in subs:
    assert a in src, 'blender_hero_ships.py changed: ' + a
    src = src.replace(a, b)
g = {'__name__': '__main__', '__file__': path}
sys.path.insert(0, TOOLS)
exec(compile(src, path, 'exec'), g)
