#!/usr/bin/env python3
"""Web half of the desktop/web parity capture (M1 webparity). Runs the same screenshot driver the desktop capture uses
(res://scenes/ui/tools/screenshot.gd) inside the web export, so both builds frame the exact same game state.

  python serve_web.py --port 8093                                   # in one shell (localhost only)
  python web_parity.py http://127.0.0.1:8093/ <out_dir> [shot ...] [--env SHOT_X=1 ...] [--dpr 1.25] [--css 1536x864]

The page's GODOT_CONFIG.args are rewritten on the fly (no file changes) to `-s screenshot.gd -- user://shots <shots> SHOT_..`.
For each shot the game prints SHOT_READY <name>; this script saves
  web-<name>.png           the page as the browser shows it (canvas after devicePixelRatio scaling and compositing)
  web-<name>-internal.png  the frame Godot read back itself (for the 3D-only close-ups, the same crop the desktop saves)
then acknowledges, and the game moves on. Real GPU (ANGLE D3D11, the same path Chrome uses for play), not SwiftShader.
Default page size: 1536x864 CSS px at devicePixelRatio 1.25 = 1920x1080 device px, the desktop capture's resolution and
Simon's 125 % Windows scaling. Writes web-parity.json (console, errors, pck requests, webgl renderer, canvas size, FPS lines).
"""
import base64
import json
import os
import re
import sys
import time

SHOTS = ["title", "galaxy", "system", "battle-plan", "battle-volley", "battle-closeup"]


def main():
    argv = sys.argv[1:]
    env, dpr, css = ["SHOT_FULL=1"], 1.25, (1536, 864)
    script = "res://scenes/ui/tools/screenshot.gd"
    rest = []
    i = 0
    while i < len(argv):
        if argv[i] == "--env":
            env.append(argv[i + 1]); i += 2
        elif argv[i] == "--script":
            script = argv[i + 1]; i += 2
        elif argv[i] == "--dpr":
            dpr = float(argv[i + 1]); i += 2
        elif argv[i] == "--css":
            w, h = argv[i + 1].split("x"); css = (int(w), int(h)); i += 2
        else:
            rest.append(argv[i]); i += 1
    url, out = rest[0], rest[1]
    shots = rest[2:] or SHOTS
    os.makedirs(out, exist_ok=True)
    # No --audio-driver Dummy here: on the web build it runs out of wasm memory at boot (asks for 3 GB). Chrome runs muted
    # (--mute-audio) and headless, so nothing plays aloud.
    args = ["-s", script, "--", "user://shots"] + shots + env
    rep = {"args": args, "console": [], "errors": [], "requests": [], "frames": [], "fps": []}
    ready = []
    from playwright.sync_api import sync_playwright
    with sync_playwright() as p:
        b = p.chromium.launch(channel="chrome", headless=True,
                              args=["--use-angle=d3d11", "--ignore-gpu-blocklist", "--mute-audio",
                                    "--disable-background-timer-throttling", "--disable-renderer-backgrounding"])
        ctx = b.new_context(viewport={"width": css[0], "height": css[1]}, device_scale_factor=dpr)
        pg = ctx.new_page()

        def on_console(m):
            t = m.text
            rep["console"].append(t)
            if t.startswith("SHOT_READY "):
                ready.append(t.split(" ", 1)[1].strip())
            if t.startswith("FPS"):
                rep["fps"].append(t)

        pg.on("console", on_console)
        pg.on("pageerror", lambda e: rep["errors"].append("pageerror: " + str(e)))
        pg.on("requestfinished", lambda r: rep["requests"].append(r.url.split("/", 3)[-1]) if ".pck" in r.url else None)

        def patch(route):
            resp = route.fetch()
            body = resp.text()
            body = re.sub(r'"args":\[[^\]]*\]', '"args":' + json.dumps(args), body, count=1)
            route.fulfill(response=resp, body=body)

        pg.route(re.compile(r".*/(index\.html)?(\?.*)?$"), patch)
        pg.goto(url, wait_until="load")
        rep["webgl"] = pg.evaluate("() => { const g=document.createElement('canvas').getContext('webgl2'); const e=g&&g.getExtension('WEBGL_debug_renderer_info'); return e?g.getParameter(e.UNMASKED_RENDERER_WEBGL):'?' }")
        done = 0
        deadline = time.time() + 600
        while done < len(shots) and time.time() < deadline:
            if not ready:
                pg.wait_for_timeout(250)
                continue
            name = ready.pop(0)
            pg.wait_for_timeout(500)
            f = os.path.join(out, "web-%s.png" % name)
            pg.screenshot(path=f)
            b64 = pg.evaluate("() => window.__shotPng || ''")
            if b64:
                open(os.path.join(out, "web-%s-internal.png" % name), "wb").write(base64.b64decode(b64))
            rep["canvas"] = pg.evaluate("() => { const c=document.querySelector('canvas'); return [c.width, c.height, c.clientWidth, c.clientHeight, devicePixelRatio] }")
            pg.evaluate("(n) => { window.__shotAck = n }", name)
            rep["frames"].append(f)
            done += 1
            print("shot", name, "saved", flush=True)
        pg.wait_for_timeout(1000)
        b.close()
    json.dump(rep, open(os.path.join(out, "web-parity.json"), "w"), indent=1)
    print("frames:", len(rep["frames"]), "of", len(shots), "| webgl:", rep.get("webgl"), "| canvas:", rep.get("canvas"))
    print("pcks:", rep["requests"])
    for c in rep["fps"]:
        print(" ", c)
    for c in rep["console"]:
        if "ERROR" in c or "SCRIPT" in c:
            print("  console:", c[:240])
    for e in rep["errors"]:
        print("  error:", e[:240])
    sys.exit(0 if len(rep["frames"]) == len(shots) else 1)


if __name__ == "__main__":
    main()
