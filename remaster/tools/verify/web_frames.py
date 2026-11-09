#!/usr/bin/env python3
"""Drives the localhost Web export in headless Chrome and saves title, galaxy, system and battle frames, plus a report of which packs the
browser fetched (planets_web.pck, audio_<empire>_web.pck) and any console errors.

  python serve_web.py --port 8060            # in one shell (localhost only)
  python web_frames.py http://127.0.0.1:8060/ <out_dir>

The game draws to one canvas, so the steps click by position at 1280x720: the Tactical drill, SYSTEM, GALAXY, Enter (end turn), TAKE COMMAND. Each step waits
for the page to settle and saves a png; the caller looks at the frames. Exit 0 when the core booted and every frame was written."""
import json
import os
import sys
import time


def main():
    url, out = sys.argv[1], sys.argv[2]
    os.makedirs(out, exist_ok=True)
    rep = {"console": [], "errors": [], "requests": [], "frames": []}
    from playwright.sync_api import sync_playwright
    with sync_playwright() as p:
        b = None
        for ch in ("chrome", "msedge", None):
            try:
                b = p.chromium.launch(channel=ch, headless=True, args=["--use-gl=angle", "--use-angle=swiftshader", "--enable-unsafe-swiftshader", "--ignore-gpu-blocklist"])
                break
            except Exception as e:
                rep["errors"].append("launch %s: %s" % (ch, str(e).splitlines()[0]))
        pg = b.new_page(viewport={"width": int(os.environ.get("VW", 1280)), "height": int(os.environ.get("VH", 720))})
        pg.on("console", lambda m: rep["console"].append(m.text))
        pg.on("pageerror", lambda e: rep["errors"].append("pageerror: " + str(e)))
        pg.on("requestfinished", lambda r: rep["requests"].append(r.url.split("/", 3)[-1]) if ".pck" in r.url else None)
        pg.goto(url, wait_until="load")
        t = time.time()
        while time.time() - t < 60 and not any("core ok=" in c for c in rep["console"]):
            time.sleep(0.5)
        time.sleep(6)

        def shot(name, wait=3):
            time.sleep(wait)
            f = os.path.join(out, name + ".png")
            pg.screenshot(path=f)
            rep["frames"].append(f)

        shot("web-title", 1)
        pg.mouse.click(216, 520)                  # Tactical drill: a ready-made skirmish at Sol, opens on the galaxy map
        shot("web-galaxy", 10)
        pg.mouse.click(73, 133)                   # SYSTEM: the planets drawn from planets_web.pck
        shot("web-system", 8)
        pg.mouse.click(73, 96)                    # back to GALAXY, then Enter = end turn: the skirmish joins
        time.sleep(2)
        pg.keyboard.press("Enter")
        shot("web-battle-contact", 8)
        pg.mouse.click(460, 431)                  # TAKE COMMAND
        shot("web-battle", 12)
        b.close()
    json.dump(rep, open(os.path.join(out, "web-frames.json"), "w"), indent=1)
    print("frames:", len(rep["frames"]), "pcks fetched:", rep["requests"], "errors:", len(rep["errors"]))
    for c in rep["console"]:
        if "ERROR" in c or "WARNING" in c:
            print("  console:", c[:200])


if __name__ == "__main__":
    main()
