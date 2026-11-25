#!/usr/bin/env python3
"""Static check of a web build's packs for the hull look (hull-look lane, 25 Nov 2026).

  python check_title_packs.py <web build dir containing packs/manifest.json> [--max-empire-mb 24]

The title fleet of every empire (TitleFleet.HEROES) and every station class must have a hero hull pack in the manifest, so the
browser title and battle never fall back to the procedural stand-in hulls, and the three title packs of one empire together must
stay under --max-empire-mb (they download behind the title). Fails (exit 1) on a missing pack or an over-budget empire. Reads
title_fleet.gd for the keys, so a new title hull is checked without editing this file.
"""
import argparse
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
FAC = {1: "fed", 2: "kli", 3: "rom", 4: "car", 5: "fer"}


def title_keys():
    src = open(os.path.join(HERE, "..", "..", "godot", "scenes", "battle", "title_fleet.gd"), encoding="utf-8").read()
    body = src[src.index("const HEROES"):src.index("const HERO_LENGTH")]
    out = {}
    for line in body.splitlines():
        m = re.match(r"\s*(\d):\s*\[\[", line)
        if m:
            out[int(m.group(1))] = re.findall(r'\["(ship\.[a-z0-9_.]+)"', line)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("build")
    ap.add_argument("--max-empire-mb", type=float, default=24.0)
    a = ap.parse_args()
    man_path = os.path.join(a.build, "packs", "manifest.json")
    man = json.load(open(man_path, encoding="utf-8"))
    files, sizes = man["files"], man["bytes"]

    def pack(key):
        name = "hull_%s_web.pck" % key.replace(".", "_")
        return name, sizes.get(files.get(name, ""), 0)

    fails = []
    keys = title_keys()
    if len(keys) != 5:
        fails.append("could not read the title fleet from title_fleet.gd (%s)" % keys)
    for e, ks in sorted(keys.items()):
        tot = 0
        for k in ks:
            name, b = pack(k)
            if b <= 0:
                fails.append("title empire %d: no hull pack for %s (the web title would draw a procedural stand-in)" % (e, k))
            tot += b
        print("title empire %d (%s): %d hulls, %.1f MB" % (e, FAC[e], len(ks), tot / 1048576.0))
        if tot > a.max_empire_mb * 1048576:
            fails.append("title empire %d: %.1f MB of hull packs is over %.0f MB" % (e, tot / 1048576.0, a.max_empire_mb))
    for e, fac in FAC.items():
        for kind in ("starbase", "outpost"):
            k = "ship.%s.%s" % (fac, kind)
            name, b = pack(k)
            print("station %s: %.1f MB" % (k, b / 1048576.0))
            if b <= 0:
                fails.append("station %s has no hull pack (the web battle would draw the procedural station)" % k)
    for f in fails:
        print("TITLEPACKS FAIL", f)
    print("TITLEPACKS summary:", "FAIL" if fails else "ok")
    sys.exit(1 if fails else 0)


if __name__ == "__main__":
    main()
