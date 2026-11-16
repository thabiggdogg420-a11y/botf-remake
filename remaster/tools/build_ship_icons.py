#!/usr/bin/env python3
"""Builds ship_icons_desktop.pck: the baked menu ship pictures, packed so a packaged game shows them with no LocalAssets.

  python remaster/tools/build_ship_icons.py [--bake] [--localassets <dir>] [--out <ship_icons_desktop.pck>] [--filter <text>]
      [--godot <console exe>] [--proj remaster/godot]

The icons (LocalAssets/ship_icons/<fac>_<class>_<iso|top>[_64].png) are renders of the hero models and so third-party
derived: the pck is a build output only, never committed. ShipIcons (scenes/ui/widgets/ship_icons.gd) reads them from
res://assets/ship_icons/ once AppLoader has mounted the pck (user://packs or <exe dir>/packs); the disk folders stay a dev
fallback.

  --bake      first re-render every icon from the current hero models (needs a GPU window: godot/tools/bake_ship_icons.gd),
              in chunks of --chunk classes, one Godot process each (a single process that renders hundreds of hulls loses the
              Vulkan device).
              Do this after the hero models change (the hullhd lane), then rebuild the pack: ONE command does both:
                  python remaster/tools/build_ship_icons.py --bake
  --filter    with --bake: only keys containing this text (e.g. "fed.galaxy"); the pack is built from whatever is on disk.

Defaults: --localassets from $BOTF_LOCALASSETS, else remaster/game/LocalAssets of this checkout or of the main checkout of the
repo (a worktree has none); --out to %APPDATA%/Godot/app_userdata/BOTF Remastered (Godot)/packs/ship_icons_desktop.pck;
--godot from $GODOT, else C:/Tools/Godot/Godot_v4.5-stable_win64_console.exe. Prints one line with the file count and size.
Exit 0 when the pack exists and holds at least one icon.
"""
import argparse
import json
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
REMASTER = os.path.dirname(HERE)
LOW = getattr(subprocess, "BELOW_NORMAL_PRIORITY_CLASS", 0)


def find_localassets(arg):
    cands = [arg, os.environ.get("BOTF_LOCALASSETS"), os.path.join(REMASTER, "game", "LocalAssets")]
    try:
        common = subprocess.run(["git", "-C", REMASTER, "rev-parse", "--git-common-dir"], capture_output=True, text=True).stdout.strip()
        if common:
            root = os.path.dirname(os.path.abspath(os.path.join(REMASTER, common)))
            cands.append(os.path.join(root, "remaster", "game", "LocalAssets"))
    except OSError:
        pass
    for c in cands:
        if c and os.path.isdir(os.path.join(c, "ships")):
            return os.path.abspath(c)
    sys.exit("no LocalAssets with a ships/ folder found (set --localassets or BOTF_LOCALASSETS)")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--bake", action="store_true")
    ap.add_argument("--localassets")
    ap.add_argument("--out")
    ap.add_argument("--web-out", help="also write ship_icons_web.pck here: the same names, PNGs re-saved optimised (F1 web budget)")
    ap.add_argument("--filter", default="")
    ap.add_argument("--chunk", type=int, default=16, help="classes per Godot launch when baking")
    ap.add_argument("--godot", default=os.environ.get("GODOT", r"C:\Tools\Godot\Godot_v4.5-stable_win64_console.exe"))
    ap.add_argument("--proj", default=os.path.join(REMASTER, "godot"))
    a = ap.parse_args()

    la = find_localassets(a.localassets)
    icons = os.path.join(la, "ship_icons")
    out = a.out or os.path.join(os.environ.get("APPDATA", ""), "Godot", "app_userdata", "BOTF Remastered (Godot)", "packs", "ship_icons_desktop.pck")
    out = os.path.abspath(out)

    if a.bake:
        os.makedirs(icons, exist_ok=True)
        env = dict(os.environ, BOTF_HERO_MANIFEST=os.path.join(la, "ships", "godot-hero-manifest.json"))
        man = os.path.join(la, "ships", "godot-hero-manifest.json")
        keys = sorted(k for k in json.load(open(man, encoding="utf-8"))["heroes"] if a.filter in k)
        failed = []
        print("baking %d classes into %s in chunks of %d" % (len(keys), icons, a.chunk))
        for start in range(0, len(keys), a.chunk):
            cmd = [a.godot, "--path", a.proj, "--resolution", "640x640", "--audio-driver", "Dummy", "-s", "res://tools/bake_ship_icons.gd", "--",
                   icons, a.filter, "--start=%d" % start, "--count=%d" % a.chunk]
            r = subprocess.run(cmd, env=env, capture_output=True, text=True, creationflags=LOW)
            tail = [l for l in r.stdout.splitlines() if l.startswith("baked ")]
            print("chunk %d-%d: exit %d  %s" % (start, min(start + a.chunk, len(keys)) - 1, r.returncode, tail[-1][:120] if tail else ""))
            if r.returncode != 0:
                failed.append(start)
        if failed:
            sys.exit("bake failed in chunks starting at %s" % failed)
        print("baked %d classes" % len(keys))
    pngs = sorted(f for f in os.listdir(icons) if f.endswith(".png")) if os.path.isdir(icons) else []
    if not pngs:
        sys.exit("no icons in %s (run with --bake)" % icons)
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with tempfile.TemporaryDirectory() as td:
        marker = os.path.join(td, "_pack.txt")
        with open(marker, "w") as f:
            f.write("ship_icons_desktop %d icons\n" % len(pngs))
        lst = os.path.join(td, "list.txt")
        with open(lst, "w", encoding="utf-8") as f:
            f.write("res://assets/ship_icons/_pack.txt|%s\n" % marker.replace(chr(92), "/"))
            for p in pngs:
                f.write("res://assets/ship_icons/%s|%s\n" % (p, os.path.join(icons, p).replace(chr(92), "/")))
        r = subprocess.run([a.godot, "--headless", "--audio-driver", "Dummy", "--path", a.proj, "-s", "res://tools/pack_list.gd", "--", lst, out],
                           capture_output=True, text=True, creationflags=LOW)
    if r.returncode != 0 or not os.path.isfile(out):
        sys.exit("pack failed:\n%s\n%s" % (r.stdout[-2000:], r.stderr[-2000:]))
    print("%s %.1f MB (%d icons)" % (os.path.basename(out), os.path.getsize(out) / 1048576.0, len(pngs)))
    if a.web_out:
        from PIL import Image
        wout = os.path.abspath(a.web_out)
        os.makedirs(os.path.dirname(wout), exist_ok=True)
        with tempfile.TemporaryDirectory() as td:
            lst = os.path.join(td, "list.txt")
            marker = os.path.join(td, "_pack.txt")
            open(marker, "w").write("ship_icons_web %d icons\n" % len(pngs))
            with open(lst, "w", encoding="utf-8") as f:
                f.write("res://assets/ship_icons/_pack.txt|%s\n" % marker.replace(chr(92), "/"))
                for p in pngs:
                    dst = os.path.join(td, p)
                    im = Image.open(os.path.join(icons, p))
                    im.save(dst, "PNG", optimize=True, compress_level=9)
                    f.write("res://assets/ship_icons/%s|%s\n" % (p, dst.replace(chr(92), "/")))
            r = subprocess.run([a.godot, "--headless", "--audio-driver", "Dummy", "--path", a.proj, "-s", "res://tools/pack_list.gd", "--", lst, wout],
                               capture_output=True, text=True, creationflags=LOW)
        if r.returncode != 0 or not os.path.isfile(wout):
            sys.exit("web pack failed:\n%s\n%s" % (r.stdout[-2000:], r.stderr[-2000:]))
        print("%s %.1f MB (%d icons)" % (os.path.basename(wout), os.path.getsize(wout) / 1048576.0, len(pngs)))


if __name__ == "__main__":
    main()
