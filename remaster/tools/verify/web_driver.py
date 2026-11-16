#!/usr/bin/env python3
"""A persistent real-browser session for playing the Godot web build by hand or by script (F1 browser lane).

  python web_driver.py --dir <web build> --work <dir> [--host-sim] [--throttle down_kbps,up_kbps,latency_ms] [--w 1280 --h 720]

Starts the localhost server (serve_web.py; --host-sim for the plain-host simulation under /botf/), opens the page in Chromium
(Playwright), then executes one command per line appended to <work>/cmd.txt. After each it appends one line to
<work>/out.txt ("<n> ok ..." or "<n> ERR ..."). Commands:
  shot <name>            screenshot to <work>/<name>.png
  click <x> <y> [right|double]
  drag <x1> <y1> <x2> <y2>
  key <Key>              Playwright key name (Enter, F2, Escape, Control+S, ...)
  type <text>
  wheel <dy>             scroll at the canvas centre
  move <x> <y>
  wait <seconds>
  log [n]                the last n console lines go to out.txt
  waitlog <text> [secs]  waits until a console line contains the text
  eval <js>              runs the JS in the page, result to out.txt
  stats                  bytes transferred (CDP encodedDataLength) and response count so far in this context
  net [n]                the last n responses (url, status, content-length)
  reload                 reloads the page (same context: IndexedDB and HTTP cache persist)
  newctx                 fresh browser context (empty cache and IndexedDB), reopens the page
  quit
"""
import argparse
import json
import os
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dir", required=True)
    ap.add_argument("--work", required=True)
    ap.add_argument("--host-sim", action="store_true")
    ap.add_argument("--gzip-all", action="store_true")
    ap.add_argument("--throttle", default="", help="down_kbps,up_kbps,latency_ms (kilobits per second)")
    ap.add_argument("--w", type=int, default=1280)
    ap.add_argument("--h", type=int, default=720)
    ap.add_argument("--channel", default="chrome")
    ap.add_argument("--query", default="")
    ap.add_argument("--env", action="append", default=[], help="K=V environment variable for the game (GODOT_CONFIG.env), e.g. BOTF_CUE_TRACE=1")
    ap.add_argument("--args", default="", help="game command-line arguments, space separated, e.g. '-s res://x.gd -- a b' (GODOT_CONFIG.args)")
    a = ap.parse_args()
    os.makedirs(a.work, exist_ok=True)
    cmdf, outf = os.path.join(a.work, "cmd.txt"), os.path.join(a.work, "out.txt")
    open(cmdf, "w").close()
    open(outf, "w").close()
    sargs = [sys.executable, os.path.join(HERE, "serve_web.py"), "--dir", a.dir, "--port", "0"]
    if a.host_sim:
        sargs.append("--host-sim")
        if a.gzip_all:
            sargs.append("--gzip-all")
    srv = subprocess.Popen(sargs, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, env=dict(os.environ, SERVE_QUIET="1"))
    line = srv.stdout.readline()
    url = line.split(" at ")[1].split(" ")[0] + a.query
    from playwright.sync_api import sync_playwright
    log = []
    net = {"bytes": 0, "files": 0, "list": []}

    def out(s):
        with open(outf, "a", encoding="utf-8") as f:
            f.write(s + "\n")

    with sync_playwright() as p:
        browser = p.chromium.launch(channel=a.channel if a.channel != "chromium" else None, headless=True,
                                    args=["--use-gl=angle", "--use-angle=swiftshader", "--enable-unsafe-swiftshader",
                                          "--ignore-gpu-blocklist", "--autoplay-policy=no-user-gesture-required", "--mute-audio"])

        def open_ctx():
            ctx = browser.new_context(viewport={"width": a.w, "height": a.h})
            pg = ctx.new_page()
            if a.env or a.args:
                import re as _re

                def patch(route):
                    resp = route.fetch()
                    body = resp.text()
                    cfg = {}
                    m = _re.search(r"const GODOT_CONFIG = (\{.*?\});", body, _re.S)
                    if m:
                        cfg = json.loads(m.group(1))
                        cfg["env"] = dict(kv.split("=", 1) for kv in a.env)
                        if a.args:
                            cfg["args"] = a.args.split(" ")
                        body = body[:m.start(1)] + json.dumps(cfg) + body[m.end(1):]
                    route.fulfill(response=resp, body=body)
                ctx.route("**/index.html*", patch)
                ctx.route("**/", patch)
            pg.on("console", lambda m: log.append(m.text))
            pg.on("pageerror", lambda e: log.append("PAGEERR " + str(e)))
            net["bytes"] = 0
            net["files"] = 0
            net["list"] = []

            def on_resp(r):
                try:
                    sz = int(r.headers.get("content-length", "0") or 0)
                except ValueError:
                    sz = 0
                net["files"] += 1
                net["list"].append((r.url.split("/", 3)[-1][:80], r.status, sz))
            pg.on("response", on_resp)
            cdp = ctx.new_cdp_session(pg)
            cdp.send("Network.enable")
            if a.throttle:
                d, u, lat = [float(x) for x in a.throttle.split(",")]
                cdp.send("Network.emulateNetworkConditions", {"offline": False, "latency": lat,
                                                              "downloadThroughput": d * 1000 / 8, "uploadThroughput": u * 1000 / 8})
            cdp.on("Network.loadingFinished", lambda e: net.__setitem__("bytes", net["bytes"] + int(e.get("encodedDataLength", 0))))
            pg.goto(url, wait_until="load")
            return ctx, pg

        ctx, pg = open_ctx()
        out("ready " + url)
        n = 0
        pos = 0
        while True:
            pg.wait_for_timeout(200)   # pumps Playwright events (console, network); time.sleep would not
            data = open(cmdf, encoding="utf-8").read().splitlines()
            if pos >= len(data):
                continue
            line = data[pos]
            pos += 1
            n += 1
            parts = line.split(" ", 1)
            c, rest = parts[0], (parts[1] if len(parts) > 1 else "")
            try:
                if c == "shot":
                    pg.screenshot(path=os.path.join(a.work, rest + ".png"))
                    out("%d ok shot %s" % (n, rest))
                elif c == "click":
                    f = rest.split()
                    x, y = float(f[0]), float(f[1])
                    if len(f) > 2 and f[2] == "right":
                        pg.mouse.move(x, y)
                        pg.wait_for_timeout(100)
                        pg.mouse.click(x, y, button="right")
                    elif len(f) > 2 and f[2] == "double":
                        pg.mouse.move(x, y)
                        pg.wait_for_timeout(100)
                        pg.mouse.dblclick(x, y)
                    else:
                        pg.mouse.move(x, y)
                        pg.wait_for_timeout(150)
                        pg.mouse.click(x, y)
                    out("%d ok click" % n)
                elif c == "move":
                    f = rest.split()
                    pg.mouse.move(float(f[0]), float(f[1]))
                    out("%d ok move" % n)
                elif c == "drag":
                    f = [float(v) for v in rest.split()]
                    pg.mouse.move(f[0], f[1])
                    pg.mouse.down()
                    pg.mouse.move(f[2], f[3], steps=8)
                    pg.mouse.up()
                    out("%d ok drag" % n)
                elif c == "key":
                    pg.keyboard.press(rest)
                    out("%d ok key" % n)
                elif c == "type":
                    pg.keyboard.type(rest)
                    out("%d ok type" % n)
                elif c == "wheel":
                    pg.mouse.move(a.w / 2, a.h / 2)
                    pg.mouse.wheel(0, float(rest))
                    out("%d ok wheel" % n)
                elif c == "wait":
                    pg.wait_for_timeout(float(rest) * 1000)
                    out("%d ok wait" % n)
                elif c == "log":
                    k = int(rest or 15)
                    out("%d ok log:\n  " % n + "\n  ".join(l[:240] for l in log[-k:]))
                elif c == "waitlog":
                    f = rest.rsplit(" ", 1)
                    text, secs = (f[0], float(f[1])) if len(f) == 2 and f[1].replace(".", "").isdigit() else (rest, 60.0)
                    t = time.time()
                    hit = ""
                    while time.time() - t < secs and not hit:
                        hit = next((l for l in log if text in l), "")
                        pg.wait_for_timeout(500)
                    out("%d %s waitlog %r after %.1fs: %s" % (n, "ok" if hit else "ERR", text, time.time() - t, hit[:200]))
                elif c == "eval":
                    out("%d ok eval %s" % (n, json.dumps(pg.evaluate(rest))[:500]))
                elif c == "stats":
                    out("%d ok stats bytes=%.2f MB responses=%d" % (n, net["bytes"] / 1048576.0, net["files"]))
                elif c == "net":
                    out("%d ok net\n  " % n + "\n  ".join("%s %s %s" % t for t in net["list"][-int(rest or 30):]))
                elif c == "reload":
                    log.clear()
                    pg.reload(wait_until="load")
                    out("%d ok reload" % n)
                elif c == "newctx":
                    log.clear()
                    ctx.close()
                    ctx, pg = open_ctx()
                    out("%d ok newctx" % n)
                elif c == "quit":
                    out("%d ok quit" % n)
                    break
                else:
                    out("%d ERR unknown %s" % (n, c))
            except Exception as e:  # keep the session alive
                out("%d ERR %s %s" % (n, c, str(e).splitlines()[0][:200]))
        browser.close()
    srv.terminate()


if __name__ == "__main__":
    main()
