#!/usr/bin/env python3
"""Chrome CPU profile of the idle title and the idle galaxy map (25 Nov 2026, Barclay).

  python perf_cpuprof.py <url> <out_dir> [--cpu N] [--secs 6] [--tag NAME]

Uses the DevTools Profiler (sampling every 100 us) over the title idle and the galaxy idle of the same flow as
perf_frames.py, and prints the top self-time functions and the share by kind: GL glue (emscripten _glXxx / WebGL calls, i.e.
what each draw call and state change costs on the main thread), wasm (engine + GDScript interpreter, unnamed in a release
build) and everything else (browser: rendering, GC, program idle). Together with perf_probe.py's draw counts this says whether a
frame is draw-call bound or script bound.
"""
import argparse
import json
import os
import shutil
import sys
import tempfile
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import perf_frames as pf  # noqa: E402


def summarise(prof, label, top=22):
    nodes = {n["id"]: n for n in prof["nodes"]}
    dt = prof["timeDeltas"]
    selfus = {}
    for nid, d in zip(prof["samples"], dt):
        selfus[nid] = selfus.get(nid, 0) + d
    total = sum(selfus.values()) or 1
    agg = {}
    kind = {"gl": 0, "wasm": 0, "idle": 0, "gc": 0, "other": 0}
    for nid, us in selfus.items():
        cf = nodes[nid]["callFrame"]
        name = cf["functionName"] or "(anonymous)"
        url = cf["url"].rsplit("/", 1)[-1]
        key = "%s %s" % (name, url)
        agg[key] = agg.get(key, 0) + us
        lname = name.lower()
        if name == "(idle)" or name == "(program)":
            kind["idle"] += us
        elif name == "(garbage collector)":
            kind["gc"] += us
        elif lname.startswith("_gl") or lname.startswith("gl") or "webgl" in lname or lname.startswith("_emscripten_gl") or lname in ("drawelements", "drawarrays", "useprogram", "uniform4fv", "bindtexture", "bufferdata", "buffersubdata"):
            kind["gl"] += us
        elif name.startswith("wasm-function") or "wasm" in url or name.startswith("$") or url.endswith(".wasm"):
            kind["wasm"] += us
        else:
            kind["other"] += us
    print("== %s: %.1f ms sampled" % (label, total / 1000.0))
    print("   share: " + ", ".join("%s %.0f%%" % (k, 100.0 * v / total) for k, v in kind.items()))
    for k, us in sorted(agg.items(), key=lambda x: -x[1])[:top]:
        print("   %6.1f ms %5.1f%%  %s" % (us / 1000.0, 100.0 * us / total, k[:110]))
    return {"total_ms": total / 1000.0, "kind_ms": {k: v / 1000.0 for k, v in kind.items()}, "top": sorted(agg.items(), key=lambda x: -x[1])[:60]}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("url")
    ap.add_argument("out")
    ap.add_argument("--cpu", type=float, default=1.0)
    ap.add_argument("--secs", type=float, default=6.0)
    ap.add_argument("--tag", default="cpuprof")
    ap.add_argument("--w", type=int, default=1280)
    ap.add_argument("--h", type=int, default=720)
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    from playwright.sync_api import sync_playwright
    sx, sy = a.w / 1280.0, a.h / 720.0
    res = {}
    with sync_playwright() as p:
        prof_dir = tempfile.mkdtemp(prefix="botf-cprof-")
        ctx = p.chromium.launch_persistent_context(prof_dir, channel="chrome", headless=False, viewport={"width": a.w, "height": a.h},
                                                   args=["--use-angle=d3d11", "--enable-unsafe-webgpu", "--ignore-gpu-blocklist", "--mute-audio", "--window-position=-10000,-10000"])
        pf.lower()
        pg = ctx.pages[0] if ctx.pages else ctx.new_page()
        cdp = ctx.new_cdp_session(pg)
        if a.cpu > 1:
            cdp.send("Emulation.setCPUThrottlingRate", {"rate": a.cpu})
        cdp.send("Profiler.enable")
        cdp.send("Profiler.setSamplingInterval", {"interval": 100})
        con = []
        pg.on("console", lambda m: con.append(m.text))
        t0 = time.time()
        pg.goto(a.url, wait_until="load")
        while time.time() - t0 < 240 * max(1.0, a.cpu) and not any("BOTF title up" in c for c in con):
            pg.wait_for_timeout(250)
        pg.wait_for_timeout(int(8000 * max(1.0, a.cpu)))   # let the title's own warm-ups finish
        for label, act in (("title-idle", None), ("galaxy-idle", True)):
            if act:
                pg.mouse.click(330 * sx, 518 * sy)
                pg.wait_for_timeout(int(3000 * max(1.0, a.cpu)))
                pg.mouse.click(1116 * sx, 658 * sy)
                pg.wait_for_timeout(int(10000 * max(1.0, a.cpu)))
                pg.mouse.click(280 * sx, 631 * sy)
                pg.wait_for_timeout(1500)
            cdp.send("Profiler.start")
            pg.wait_for_timeout(int(a.secs * 1000))
            prof = cdp.send("Profiler.stop")["profile"]
            res[label] = summarise(prof, label)
        json.dump(res, open(os.path.join(a.out, a.tag + ".json"), "w"), indent=1)
        ctx.close()
        shutil.rmtree(prof_dir, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
