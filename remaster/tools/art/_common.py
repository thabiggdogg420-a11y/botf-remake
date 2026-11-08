"""Shared paths for the L7 art tools. LocalAssets lives in the main checkout (gitignored), not in the worktrees."""
import json
import os
import subprocess

HERE = os.path.dirname(os.path.abspath(__file__))
REMASTER = os.path.abspath(os.path.join(HERE, '..', '..'))
MAIN_DEFAULT = r'C:\Users\BiggDogg\Documents\ChatGPT\Game Remakes\remaster'
MAIN = os.environ.get('BOTF_MAIN_REMASTER', MAIN_DEFAULT)
LOCAL = os.environ.get('BOTF_LOCAL_ASSETS', os.path.join(MAIN, 'game', 'LocalAssets'))
BLENDER = os.environ.get('BLENDER', r'C:\Program Files\Blender Foundation\Blender 5.2\blender.exe')
BELOW_NORMAL = 0x00004000  # Windows BELOW_NORMAL_PRIORITY_CLASS


def run_below_normal(cmd, **kw):
    return subprocess.run(cmd, creationflags=BELOW_NORMAL, **kw)


def load_json(p):
    with open(p, encoding='utf-8-sig') as f:
        return json.load(f)
