#!/usr/bin/env python3
"""Phone-emulation frames of the Web export in headless Chrome (mobile stage M-A, Barclay, 25 Nov 2026).

  python mobile_frames.py <web_build_dir> <out_dir> [--size 844x390] [--dpr 2] [--tag m844]

Serves the build with serve_web.py on localhost, opens it as a touch phone (Chrome device emulation: has_touch, is_mobile,
the given landscape size and device pixel ratio) and writes:
  <tag>-site-loading.png   the host page while the game loads: the Desktop | Mobile switch, Mobile marked
  <tag>-title.png          the title in mobile mode
  <tag>-switch.png         the switch opened over the running game (tap on its round tab)
  <tag>-desktop-site-loading.png  ?mode=desktop while loading: the switch with Desktop marked
  <tag>-desktop-page.png   ?mode=desktop: the game as on a desktop (the switch folded to its tab)
  <tag>-desktop-switch.png the switch opened over that page
  with --play --steps '[...]': <tag>-play-<name>.png for each ["shot",name] / ["hold",x,y,secs,name] step
  <tag>-report.json        console lines, page errors, BOTF_MODE as the game saw it, the boot line
Taps use the touchscreen (page.touchscreen.tap), never the mouse. Exit 0 when the core booted in both modes."""
import argparse
import json
import os
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))


def swipe(cdp, x1, y1, x2, y2, steps=12):
    """One finger dragged from (x1,y1) to (x2,y2) as real touch events (Chrome DevTools Input.dispatchTouchEvent)."""
    cdp.send("Input.dispatchTouchEvent", {"type": "touchStart", "touchPoints": [{"x": x1, "y": y1}]})
    for i in range(1, steps + 1):
        cdp.send("Input.dispatchTouchEvent", {"type": "touchMove", "touchPoints": [{"x": x1 + (x2 - x1) * i / steps, "y": y1 + (y2 - y1) * i / steps}]})
        time.sleep(0.03)
    cdp.send("Input.dispatchTouchEvent", {"type": "touchEnd", "touchPoints": []})


def hold(cdp, x, y, secs):
    cdp.send("Input.dispatchTouchEvent", {"type": "touchStart", "touchPoints": [{"x": x, "y": y}]})
    time.sleep(secs)


def release(cdp):
    cdp.send("Input.dispatchTouchEvent", {"type": "touchEnd", "touchPoints": []})


def pinch(cdp, cx, cy, d0, d1, steps=10):
    pts = lambda d: [{"x": cx - d, "y": cy, "id": 1}, {"x": cx + d, "y": cy, "id": 2}]
    cdp.send("Input.dispatchTouchEvent", {"type": "touchStart", "touchPoints": pts(d0)})
    for i in range(1, steps + 1):
        cdp.send("Input.dispatchTouchEvent", {"type": "touchMove", "touchPoints": pts(d0 + (d1 - d0) * i / steps)})
        time.sleep(0.05)
    cdp.send("Input.dispatchTouchEvent", {"type": "touchEnd", "touchPoints": []})


def play(pg, ctx, a, w, h, rep, shot):
    """Drives the running game by touch in the phone window from a --steps JSON list (CSS pixels of the page):
    ["swipe",x1,y1,x2,y2] ["tap",x,y] ["wait",secs] ["shot",name] ["hold",x,y,secs,name] ["pinch",cx,cy,d0,d1]."""
    cdp = ctx.new_cdp_session(pg)
    raw = a.steps or "[]"
    if raw.startswith("@"):
        raw = open(raw[1:], encoding="utf-8").read()
    for st in json.loads(raw):
        op = st[0]
        if op == "swipe":
            swipe(cdp, *st[1:5])
        elif op == "tap":
            pg.touchscreen.tap(st[1], st[2])
        elif op == "wait":
            pg.wait_for_timeout(int(st[1] * 1000))
        elif op == "shot":
            shot(pg, "play-" + st[1], 1.0)
        elif op == "hold":
            hold(cdp, st[1], st[2], st[3])
            shot(pg, "play-" + st[4], 0.2)
            release(cdp)
        elif op == "pinch":
            pinch(cdp, *st[1:5])
    rep["play"] = raw


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("build")
    ap.add_argument("out")
    ap.add_argument("--size", default="844x390")
    ap.add_argument("--dpr", type=float, default=2.0)
    ap.add_argument("--tag", default="m844")
    ap.add_argument("--wait", type=int, default=240)
    ap.add_argument("--play", action="store_true", help="also drive the game by touch (see --steps)")
    ap.add_argument("--steps", default="", help="JSON list of touch steps for --play, or @file holding it")
    ap.add_argument("--skip-desktop", action="store_true", help="skip the ?mode=desktop page")
    a = ap.parse_args()
    w, h = (int(x) for x in a.size.split("x"))
    os.makedirs(a.out, exist_ok=True)
    srv = subprocess.Popen([sys.executable, os.path.join(HERE, "serve_web.py"), "--dir", a.build, "--port", "0"],
                           stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, env=dict(os.environ, SERVE_QUIET="1"))
    line = srv.stdout.readline()
    if "http://127.0.0.1:" not in line:
        print("server failed:", line)
        return 2
    base = line.split(" at ")[1].split(" ")[0]
    rep = {"base": base, "console": [], "errors": [], "mode_seen": {}, "booted": {}}
    from playwright.sync_api import sync_playwright
    try:
        with sync_playwright() as p:
            b = None
            for ch in ("chrome", "msedge", None):
                try:
                    b = p.chromium.launch(channel=ch, headless=True, args=["--mute-audio", "--use-angle=d3d11", "--enable-gpu", "--ignore-gpu-blocklist"])
                    break
                except Exception as e:  # noqa: BLE001
                    rep["errors"].append("launch %s: %s" % (ch, str(e).splitlines()[0]))
            ua = "Mozilla/5.0 (Linux; Android 13; Pixel 7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/130.0 Mobile Safari/537.36"

            def open_page(query: str, touch: bool):
                ctx = b.new_context(viewport={"width": w, "height": h}, device_scale_factor=a.dpr, is_mobile=touch, has_touch=touch,
                                    user_agent=ua if touch else None, screen={"width": w, "height": h})
                pg = ctx.new_page()
                pg.on("console", lambda m: rep["console"].append(m.text))
                pg.on("pageerror", lambda e: rep["errors"].append("pageerror: " + str(e)))
                pg.goto(base + query, wait_until="load")
                return ctx, pg

            def wait_boot(pg, key: str) -> bool:
                t = time.time()
                while time.time() - t < a.wait:
                    if any("BOTF title up" in c for c in rep["console"]):
                        rep["booted"][key] = True
                        return True
                    pg.wait_for_timeout(500)   # not time.sleep: Playwright only delivers console events while it is called
                rep["booted"][key] = False
                return False

            def shot(pg, name: str, wait: float = 1.0) -> None:
                pg.wait_for_timeout(int(wait * 1000))
                pg.screenshot(path=os.path.join(a.out, "%s-%s.png" % (a.tag, name)))

            # Mobile: an unsaved phone, no ?mode= at all: touch + narrow screen must pick Mobile by itself.
            ctx, pg = open_page("", True)
            pg.wait_for_timeout(1000)
            rep["mode_seen"]["phone-auto"] = pg.evaluate("[globalThis.BOTF_MODE, globalThis.BOTF_MODE_SRC]")
            shot(pg, "site-loading", 0.5)
            ok = wait_boot(pg, "mobile")
            pg.wait_for_timeout(3000)
            shot(pg, "title", 1.0)
            if pg.query_selector("#botf-mode"):
                bb = pg.query_selector("#botf-mode").bounding_box()
                pg.touchscreen.tap(bb["x"] + bb["width"] / 2, bb["y"] + bb["height"] / 2)
                shot(pg, "switch", 0.6)
            if a.play and ok:
                play(pg, ctx, a, w, h, rep, shot)
            ctx.close()
            if a.skip_desktop:
                b.close()
                json.dump(rep, open(os.path.join(a.out, "%s-report.json" % a.tag), "w"), indent=1)
                print(json.dumps({"mode_seen": rep["mode_seen"], "booted": rep["booted"], "errors": rep["errors"][:5]}))
                return 0 if all(rep["booted"].values()) else 1
            # Desktop at the same size, forced by the address.
            rep["console"].clear()
            ctx, pg = open_page("?mode=desktop", True)
            pg.wait_for_timeout(1000)
            rep["mode_seen"]["phone-query-desktop"] = pg.evaluate("[globalThis.BOTF_MODE, globalThis.BOTF_MODE_SRC]")
            shot(pg, "desktop-site-loading", 0.2)
            ok2 = wait_boot(pg, "desktop")
            pg.wait_for_timeout(3000)
            shot(pg, "desktop-page", 1.0)
            if pg.query_selector("#botf-mode"):
                bb = pg.query_selector("#botf-mode").bounding_box()
                pg.touchscreen.tap(bb["x"] + bb["width"] / 2, bb["y"] + bb["height"] / 2)
                shot(pg, "desktop-switch", 0.6)
            ctx.close()
            b.close()
    finally:
        srv.terminate()
    json.dump(rep, open(os.path.join(a.out, "%s-report.json" % a.tag), "w"), indent=1)
    print(json.dumps({"mode_seen": rep["mode_seen"], "booted": rep["booted"], "errors": rep["errors"][:5]}))
    return 0 if all(rep["booted"].values()) and rep["booted"] else 1


if __name__ == "__main__":
    sys.exit(main())
