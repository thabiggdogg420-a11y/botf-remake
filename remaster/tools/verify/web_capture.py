#!/usr/bin/env python3
"""Captures the Web export in headless Chromium (Playwright, using the installed Chrome or Edge).
  python web_capture.py <web_build_dir> <out_dir> [--wait 25]
Serves the build with serve_web.py on a free localhost port, opens it, waits for the game's boot line, saves a screenshot
and one after a click, and writes web-report.json: console lines, page errors, the boot line, and whether the page was
cross-origin isolated. While the wasm GDExtension core is missing the boot fails with a BotfCore / GDExtension error; the
report carries that first failure. Exit 0 only if the game booted with 'core ok=true'."""
import argparse
import json
import os
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("build")
    ap.add_argument("out")
    ap.add_argument("--wait", type=int, default=25)
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    env = dict(os.environ, SERVE_QUIET="1")
    srv = subprocess.Popen([sys.executable, os.path.join(HERE, "serve_web.py"), "--dir", a.build, "--port", "0"],
                           stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, env=env)
    line = srv.stdout.readline()
    if "http://127.0.0.1:" not in line:
        print("server failed:", line)
        sys.exit(2)
    url = line.split(" at ")[1].split(" ")[0]
    report = {"url": url, "console": [], "errors": [], "boot": "", "isolated": None, "booted": False}
    try:
        from playwright.sync_api import sync_playwright
        with sync_playwright() as p:
            browser = None
            for channel in ("chrome", "msedge", None):
                try:
                    browser = p.chromium.launch(channel=channel, headless=True,
                                                args=["--use-gl=angle", "--use-angle=swiftshader", "--enable-unsafe-swiftshader",
                                                      "--ignore-gpu-blocklist", "--autoplay-policy=no-user-gesture-required"])
                    report["browser"] = channel or "playwright-chromium"
                    break
                except Exception as e:  # try the next channel
                    report["errors"].append("launch %s: %s" % (channel, str(e).splitlines()[0]))
            if browser is None:
                raise RuntimeError("no browser could launch")
            page = browser.new_page(viewport={"width": 1280, "height": 720})
            page.on("console", lambda m: report["console"].append(m.text))
            page.on("pageerror", lambda e: report["errors"].append("pageerror: " + str(e)))
            page.goto(url, wait_until="load")
            report["isolated"] = page.evaluate("() => ({isolated: self.crossOriginIsolated, sab: typeof SharedArrayBuffer !== 'undefined'})")
            deadline = time.time() + a.wait
            while time.time() < deadline:
                if any("core ok=" in c for c in report["console"]):
                    break
                time.sleep(0.5)
            time.sleep(2)
            page.screenshot(path=os.path.join(a.out, "web-boot.png"))
            try:
                page.mouse.click(640, 360)
                time.sleep(2)
                page.screenshot(path=os.path.join(a.out, "web-after-click.png"))
            except Exception as e:
                report["errors"].append("click: " + str(e))
            browser.close()
    except Exception as e:
        report["errors"].append("capture: %s" % e)
    finally:
        srv.terminate()
    boots = [c for c in report["console"] if "core ok=" in c]
    report["boot"] = boots[0] if boots else ""
    report["booted"] = "core ok=true" in report["boot"]
    json.dump(report, open(os.path.join(a.out, "web-report.json"), "w"), indent=1)
    print("web: booted=%s boot=%r errors=%d" % (report["booted"], report["boot"], len(report["errors"])))
    for c in report["console"][:6]:
        print("  console:", c[:200])
    for e in report["errors"][:6]:
        print("  error:", e[:300])
    sys.exit(0 if report["booted"] else 1)


if __name__ == "__main__":
    main()
