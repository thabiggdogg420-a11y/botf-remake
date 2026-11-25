#!/usr/bin/env python3
"""Frames of the web build's title (and optionally a starbase battle) at 1920x1080 in Chrome, for the hull look proof
(hull-look lane, 25 Nov 2026).

  python web_title_shots.py <build_dir> <page> <out_dir> [--empires 1,2,3,4,5] [--tag webgl] [--wait 45] [--gpu webgl|webgpu]

<build_dir> is served with serve_web.py on a free 127.0.0.1 port; <page> is the path of the page to open under it (index.html,
webgl/index.html, webgpu/index.html). The title's power is pinned by feeding the engine the user argument
BOTF_TITLE_EMPIRE=<n> (the page's own start-up arguments are rewritten in flight; no file is changed). Each empire gets a fresh
profile, waits for the game's "BOTF title up" line, then takes full-page frames at +3 s (what a visitor sees first), +12 s and
after --wait seconds (the hero hulls have arrived by then). Frames land in <out_dir>/<tag>-title-e<n>-<seconds>.png. A line per
frame reports how many hero-hull packs the page had fetched by then (packs/hull_ship_* requests), so a title that never
downloaded its ships shows as 0.
"""
import argparse
import json
import os
import re
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
FAC = {1: "fed", 2: "kli", 3: "rom", 4: "car", 5: "fer"}


def title_keys():
    """{empire: [asset keys]} from TitleFleet.HEROES, so a changed title hull is checked without editing this file."""
    src = open(os.path.join(HERE, "..", "..", "godot", "scenes", "battle", "title_fleet.gd"), encoding="utf-8").read()
    body = src[src.index("const HEROES"):src.index("const HERO_LENGTH")]
    out = {}
    for line in body.splitlines():
        m = re.match(r"\s*(\d):\s*\[\[", line)
        if m:
            out[int(m.group(1))] = re.findall(r'\["(ship\.[a-z0-9_.]+)"', line)
    return out


def main():
    fails = []
    ap = argparse.ArgumentParser()
    ap.add_argument("build")
    ap.add_argument("page")
    ap.add_argument("out")
    ap.add_argument("--empires", default="1,2,3,4,5")
    ap.add_argument("--tag", default="web")
    ap.add_argument("--wait", type=int, default=45)
    ap.add_argument("--gpu", default="webgl", choices=["webgl", "webgpu"])
    ap.add_argument("--assert", dest="assert_", action="store_true",
                    help="exit 1 unless every empire's three title hero hull packs were fetched by the last title frame (and, with --battle, the station's)")
    ap.add_argument("--battle", action="store_true", help="after the title frames, click Starbase drill and take battle frames (a station fight)")
    ap.add_argument("--battle-click", default="640,946;w10;1790,40;w6;1790,40", help="clicks (x,y) and waits (wN seconds), ';' separated, from the title: Starbase drill, then END TURN so the station fight opens (1920x1080 layout)")
    ap.add_argument("--w", type=int, default=1920)
    ap.add_argument("--h", type=int, default=1080)
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    srv = None
    try:
        from playwright.sync_api import sync_playwright
        with sync_playwright() as p:
            for e in [int(x) for x in a.empires.split(",")]:
                if srv is not None:
                    srv.terminate()
                env = dict(os.environ, SERVE_QUIET="1", SERVE_INJECT_ARGS="BOTF_TITLE_EMPIRE=%d" % e)
                srv = subprocess.Popen([sys.executable, os.path.join(HERE, "serve_web.py"), "--dir", a.build, "--port", "0"],
                                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, env=env,
                                       creationflags=0x00004000)  # BELOW_NORMAL_PRIORITY_CLASS
                line = srv.stdout.readline()
                if "http://127.0.0.1:" not in line:
                    print("server failed:", line)
                    sys.exit(2)
                base = line.split(" at ")[1].split(" ")[0].rstrip("/")
                args = ["--use-angle=d3d11", "--enable-gpu", "--ignore-gpu-blocklist", "--window-position=-3000,0",
                        "--mute-audio", "--autoplay-policy=user-gesture-required"]
                if a.gpu == "webgpu":
                    args += ["--enable-unsafe-webgpu", "--enable-features=WebGPU,Vulkan"]
                browser = None
                for channel in ("chrome", "msedge"):
                    try:
                        browser = p.chromium.launch(channel=channel, headless=False, args=args)
                        break
                    except Exception as ex:
                        print("launch", channel, str(ex).splitlines()[0])
                if browser is None:
                    sys.exit(3)
                ctx = browser.new_context(viewport={"width": a.w, "height": a.h}, device_scale_factor=1)
                page = ctx.new_page()
                con = []
                reqs = []
                page.on("console", lambda m: con.append(m.text))
                page.on("request", lambda r: reqs.append(r.url))

                t0 = time.time()
                page.goto("%s/%s" % (base, a.page.lstrip("/")), wait_until="commit", timeout=120000)
                up = None
                while time.time() - t0 < 60:
                    if any("BOTF title up" in c for c in con):
                        up = time.time()
                        break
                    time.sleep(0.5)
                if up is None:
                    # The console line is not always forwarded; the title is up a few seconds after the page loads.
                    time.sleep(8)
                    up = time.time() - 8
                for sec in (3, 12, a.wait):
                    while time.time() - up < sec:
                        time.sleep(0.25)
                    shot = os.path.join(a.out, "%s-title-e%d-%02ds.png" % (a.tag, e, sec))
                    page.screenshot(path=shot)
                    hulls = sorted(set(re.sub(r".*/(hull_ship_[a-z0-9_]+?)\.[0-9a-f]{12}\.pck.*", r"\1", u) for u in reqs if "hull_ship_" in u))
                    print("e%d +%ds -> %s | hero hull packs fetched: %d %s" % (e, sec, os.path.basename(shot), len(hulls), hulls[:6]))
                want = ["hull_" + k.replace(".", "_") for k in title_keys().get(e, [])]
                missing = [w for w in want if not any(h == w or h.startswith(w + "_web") for h in hulls)]
                if missing:
                    fails.append("empire %d: title hero hull packs never fetched: %s (the title drew procedural stand-ins)" % (e, missing))
                if a.battle:
                    for step in a.battle_click.split(";"):
                        if step.startswith("w"):
                            time.sleep(float(step[1:]))
                        else:
                            cx, cy = [int(v) for v in step.split(",")]
                            page.mouse.click(cx, cy)
                            time.sleep(1.0)
                    t1 = time.time()
                    for sec in (10, 25, 45, 70):
                        while time.time() - t1 < sec:
                            time.sleep(0.25)
                        shot = os.path.join(a.out, "%s-battle-starbase-e%d-%02ds.png" % (a.tag, e, sec))
                        page.screenshot(path=shot)
                        hulls = sorted(set(re.sub(r".*/(hull_ship_[a-z0-9_]+?)\.[0-9a-f]{12}\.pck.*", r"\1", u) for u in reqs if "hull_ship_" in u))
                        print("e%d battle +%ds -> %s | hull packs fetched: %d %s" % (e, sec, os.path.basename(shot), len(hulls), hulls[:8]))
                if a.battle and a.assert_ and not any(re.match(r"hull_ship_[a-z]+_(starbase|outpost)", h) for h in hulls):
                    fails.append("empire %d: starbase battle never fetched its station hull pack" % e)
                with open(os.path.join(a.out, "%s-title-e%d-console.txt" % (a.tag, e)), "w", encoding="utf-8") as f:
                    f.write("\n".join(con[-300:]))
                browser.close()
    finally:
        if srv is not None:
            srv.terminate()
    for f in fails:
        print("WEBTITLE FAIL", f)
    print("WEBTITLE summary:", "FAIL" if fails else "ok")
    if a.assert_ and fails:
        sys.exit(1)


if __name__ == "__main__":
    main()
