#!/usr/bin/env python3
"""Per-frame present timing of the browser build from the title to the Federation galaxy map (25 Nov 2026, Barclay).

  python perf_frames.py <url> <out_dir> [--scenario cold|warm|reload] [--cpu 1|4|6] [--runs N] [--tag NAME]
                        [--w 1280 --h 720] [--phase-s 6] [--preload] [--pass-bar]

A headed Chrome (installed channel, ANGLE D3D11, window parked off-screen, BelowNormal priority, muted) is driven through
  boot -> title idle -> title mouse movement -> NEW CAMPAIGN screen -> LAUNCH CAMPAIGN (Federation) -> galaxy settle ->
  galaxy idle -> galaxy mouse movement
and every requestAnimationFrame callback is timed (the gap between two callbacks is the present-to-present time the page
sees; Chrome delivers rAF once per presented frame). Long main-thread tasks (PerformanceObserver "longtask") are kept too.
Scenarios: cold = fresh profile (empty HTTP cache, IndexedDB, shader cache); warm = a second launch on the profile a cold run
left behind; reload = a cold run, then page.reload() on the same session and the whole flow again. --cpu N sets Chrome's
CPU throttle (Emulation.setCPUThrottlingRate; 4 = a mid-range machine). --preload clicks "Resume preload" on the title and
keeps it running while the phases are measured (the tester waited for it). The URL may carry ?build=webgl|webgpu for a Web-Next
loader. Writes <tag>-run<n>.json (frames, long tasks, console, per-phase stats), PNGs of the title, the new-campaign screen and
the galaxy, and prints one table per run.

Phase stats are p50/p95/p99, worst, frames over 33 ms and the share over 33 ms. The boot phase (page load to "BOTF title up") is
reported as time only. --pass-bar applies the bar below to the steady phases (title idle/move, galaxy idle/move) and exits 1
on a miss (limits in bar_for: at --cpu 4 median <= 35 ms, p95 <= 120, p99 <= 250, <= 15 % over 33 ms, worst <= 500);
at --cpu 4 the bar is scaled (see bar_for).
"""
import argparse
import json
import os
import shutil
import sys
import tempfile
import time

REC = """
(() => {
  window.__fr = []; window.__lt = []; let last = performance.now();
  const tick = (t) => { window.__fr.push([t, t - last]); last = t; requestAnimationFrame(tick); };
  requestAnimationFrame(tick);
  try { new PerformanceObserver((l) => { for (const e of l.getEntries()) window.__lt.push([e.startTime, e.duration]); }).observe({entryTypes: ['longtask']}); } catch (e) {}
})();
"""

STEADY = ["title-idle", "title-move", "galaxy-idle", "galaxy-move"]


def lower():
    try:
        import psutil
        for p in psutil.process_iter(["name"]):
            n = (p.info["name"] or "").lower()
            if n.startswith("chrome"):
                try:
                    p.nice(psutil.BELOW_NORMAL_PRIORITY_CLASS)
                except Exception:
                    pass
    except Exception:
        pass


def pct(v, q):
    if not v:
        return 0.0
    s = sorted(v)
    k = min(len(s) - 1, int(round(q * (len(s) - 1))))
    return s[k]


def stats(frames, t0, t1):
    seg = [d for (t, d) in frames if t0 < t <= t1]
    n = len(seg)
    over = sum(1 for d in seg if d > 33.4)
    return {"n": n, "p50": round(pct(seg, .5), 1), "p95": round(pct(seg, .95), 1), "p99": round(pct(seg, .99), 1),
            "worst": round(max(seg), 1) if seg else 0.0, "over33": over, "over33_pct": round(100.0 * over / n, 1) if n else 0.0,
            "worst5": [round(d) for d in sorted(seg, reverse=True)[:5]]}


def sweep(pg, pts, steps_each=12, delay=16):
    """Mouse movement through points at roughly 60 events a second."""
    for i in range(len(pts) - 1):
        (x0, y0), (x1, y1) = pts[i], pts[i + 1]
        for s in range(1, steps_each + 1):
            pg.mouse.move(x0 + (x1 - x0) * s / steps_each, y0 + (y1 - y0) * s / steps_each)
            pg.wait_for_timeout(delay)


def run_flow(pg, a, con, tag, run, t_nav):
    """Drives one title-to-galaxy pass on page pg; returns (marks, up_s). marks = name -> (t0, t1) in page performance.now()."""
    W, H = a.w, a.h
    sx, sy = W / 1280.0, H / 720.0
    marks = {}
    up = None
    while time.time() - t_nav < 240 * max(1.0, a.cpu):
        if any("BOTF title up" in c[1] for c in con):
            up = time.time() - t_nav
            break
        pg.wait_for_timeout(250)
    marks["boot"] = (0.0, pg.evaluate("performance.now()"))
    if up is None:
        print("  title never came up")
        return marks, up
    now = lambda: pg.evaluate("performance.now()")
    if a.preload:
        pg.mouse.click(1159 * sx, 22 * sy)
        pg.wait_for_timeout(500)
    if a.settle_s > 0:   # a first visit's own warm-ups (shader compiles, pack mounts) run for the first seconds: measured, not barred
        t = now(); pg.wait_for_timeout(int(a.settle_s * 1000)); marks["title-settle"] = (t, now())
    cen = getattr(a, "census", False)
    if cen:
        pg.evaluate("window.__cen = {}; window.__hist = {}; window.__cenOn = true")
    t = now(); pg.wait_for_timeout(int(a.phase_s * 1000)); marks["title-idle"] = (t, now())
    if cen:
        a.cen_out = {"hist_title": pg.evaluate("window.__hist"), "title": pg.evaluate("(() => { window.__cenOn = false; return window.__cen; })()"), "gq_title": pg.evaluate("window.__gq")}
    pg.screenshot(path=os.path.join(a.out, "%s-run%d-title.png" % (tag, run)))
    t = now()
    btn = [(330, 518), (330, 560), (200, 597), (420, 597), (200, 630), (420, 630), (200, 663), (330, 518), (330, 560)]
    sweep(pg, [(x * sx, y * sy) for x, y in btn], 8, 16)
    sweep(pg, [(600 * sx, 300 * sy), (900 * sx, 400 * sy), (700 * sx, 200 * sy), (330 * sx, 518 * sy)], 20, 16)
    marks["title-move"] = (t, now())
    t = now()
    pg.mouse.click(330 * sx, 518 * sy)
    pg.wait_for_timeout(int(a.phase_s * 1000))
    pg.screenshot(path=os.path.join(a.out, "%s-run%d-newcampaign.png" % (tag, run)))
    marks["new-campaign"] = (t, now())
    t = now()
    pg.mouse.click(1116 * sx, 658 * sy)    # LAUNCH CAMPAIGN (Federation is preselected)
    pg.wait_for_timeout(int(a.phase_s * 1000))
    marks["galaxy-open"] = (t, now())
    t = now()
    # dismiss the tutorial card ("Skip tutorial") so it does not shade the sweep; it is a plain button at the bottom left
    pg.mouse.click(280 * sx, 631 * sy)
    pg.wait_for_timeout(1000)
    pg.screenshot(path=os.path.join(a.out, "%s-run%d-galaxy.png" % (tag, run)))
    if a.settle_s > 0:   # the battle view's warm-up and the ship-pack mounts start once the galaxy is up
        t = now(); pg.wait_for_timeout(int(min(a.settle_s, 15) * 1000)); marks["galaxy-settle"] = (t, now())
    if cen:
        pg.evaluate("window.__cen = {}; window.__hist = {}; window.__gq = {}; window.__cenOn = true")
    if getattr(a, "skip", ""):
        pg.evaluate("window.__skip = (fbo, prog) => (%s)" % a.skip)
    t = now(); pg.wait_for_timeout(int(a.phase_s * 1000)); marks["galaxy-idle"] = (t, now())
    if cen:
        a.cen_out["galaxy"] = pg.evaluate("(() => { window.__cenOn = false; return window.__cen; })()")
        a.cen_out["gq_galaxy"] = pg.evaluate("window.__gq")
        a.cen_out["hist_galaxy"] = pg.evaluate("window.__hist")
        a.cen_out["srcs"] = pg.evaluate("window.__srcs")
        a.cen_out["frames"] = {"title": len([1 for f in pg.evaluate('window.__fr') if marks['title-idle'][0] < f[0] <= marks['title-idle'][1]]),
                               "galaxy": len([1 for f in pg.evaluate('window.__fr') if marks['galaxy-idle'][0] < f[0] <= marks['galaxy-idle'][1]])}
    t = now()
    stars = [(594, 184), (525, 203), (404, 248), (686, 177), (412, 361), (627, 499), (885, 316), (540, 380), (343, 270), (730, 300), (480, 540), (820, 200)]
    sweep(pg, [(x * sx, y * sy) for x, y in stars], 14, 16)
    sweep(pg, [(x * sx, y * sy) for x, y in stars[::-1]], 14, 16)
    marks["galaxy-move"] = (t, now())
    pg.screenshot(path=os.path.join(a.out, "%s-run%d-galaxy-end.png" % (tag, run)))
    return marks, up


def bar_for(cpu):
    """The pass bar for the steady phases (title idle + mouse, galaxy idle + mouse). verify.ps1 runs it at --cpu 4, Chrome's 4x
    CPU throttle (a mid-range machine): median frame <= 35 ms, p95 <= 120 ms, p99 <= 250 ms, <= 15 % of frames over 33 ms, no
    frame over 500 ms. 0.1.574 (WebGL) failed it on the galaxy (p50 40-50 ms, 86-91 % of frames over 33 ms); 0.1.579 passes
    (p50 20 ms, 6 %). At --cpu 1 on this 100 Hz machine 0.1.574 already managed 50 fps on a quiet PC (galaxy 20 ms), so the cpu 1
    bar is only a sanity floor: p50 <= 25, p95 <= 40, p99 <= 60, <= 5 % over 33 ms, worst <= 250."""
    if cpu <= 1:
        return {"p50": 25.0, "p95": 40.0, "p99": 60.0, "over33_pct": 5.0, "worst": 250.0}
    return {"p50": 35.0, "p95": 120.0, "p99": 250.0, "over33_pct": 15.0, "worst": 500.0}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("url")
    ap.add_argument("out")
    ap.add_argument("--scenario", choices=["cold", "warm", "reload"], default="cold")
    ap.add_argument("--cpu", type=float, default=1.0)
    ap.add_argument("--runs", type=int, default=1)
    ap.add_argument("--tag", default="")
    ap.add_argument("--w", type=int, default=1280)
    ap.add_argument("--h", type=int, default=720)
    ap.add_argument("--phase-s", type=float, default=6.0)
    ap.add_argument("--preload", action="store_true")
    ap.add_argument("--settle-s", type=float, default=0.0, help="wait this long after the title is up before the title-idle window (the first visit's warm-ups land here; reported as title-settle and galaxy-settle, outside the bar)")
    ap.add_argument("--pass-bar", action="store_true")
    ap.add_argument("--keep-profile", default="", help="reuse/keep this profile dir (a warm run needs the cold run's dir)")
    ap.add_argument("--default-flags", action="store_true", help="launch Chrome with its default switches (only --mute-audio and the off-screen position added)")
    ap.add_argument("--onscreen", action="store_true", help="do not park the window off-screen")
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    server = None
    if os.path.isdir(a.url):   # a build folder: serve it here on a free local port (serve_web.py, 127.0.0.1 only)
        import subprocess
        server = subprocess.Popen([sys.executable, "-u", os.path.join(os.path.dirname(os.path.abspath(__file__)), "serve_web.py"), "--dir", a.url, "--port", "0"],
                                  stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        line = server.stdout.readline()
        import re
        m = re.search(r"(http://127\.0\.0\.1:\d+/)", line)
        if not m:
            server.kill(); print("could not start the local server: " + line); return 2
        a.url = m.group(1)
    try:
        return _main(a)
    finally:
        if server is not None:
            server.kill()


def _main(a):
    from playwright.sync_api import sync_playwright
    failed = False
    with sync_playwright() as p:
        for run in range(1, a.runs + 1):
            prof = a.keep_profile or tempfile.mkdtemp(prefix="botf-perf-")
            if a.scenario == "warm" and not a.keep_profile:
                print("warm needs --keep-profile (the dir a cold run left)"); return 2
            # default-flags: what a visitor's Chrome runs (no ANGLE override, no unsafe-WebGPU or blocklist switches); the older
            # runs used the verify tools' flags below, which this machine's Chrome treats the same.
            args = ["--mute-audio"] if a.default_flags else ["--use-angle=d3d11", "--enable-unsafe-webgpu", "--ignore-gpu-blocklist", "--mute-audio"]
            if not a.onscreen:
                args.append("--window-position=-10000,-10000")
            ctx = p.chromium.launch_persistent_context(prof, channel="chrome", headless=False, args=args,
                                                       viewport={"width": a.w, "height": a.h})
            lower()
            ctx.add_init_script(REC)
            pg = ctx.pages[0] if ctx.pages else ctx.new_page()
            cdp = ctx.new_cdp_session(pg)
            if a.cpu > 1:
                cdp.send("Emulation.setCPUThrottlingRate", {"rate": a.cpu})
            con = []
            pg.on("console", lambda m: con.append([time.time(), m.text]))
            tag = a.tag or "run"
            passes = [("first", None)]
            if a.scenario == "reload":
                passes.append(("reload", True))
            results = {}
            for label, is_reload in passes:
                del con[:]
                t_nav = time.time()
                if is_reload:
                    pg.reload(wait_until="load")
                else:
                    pg.goto(a.url, wait_until="load")
                # the page is re-created on a reload/redirect: the init script re-installs the recorder; give it a tick
                marks, up = run_flow(pg, a, con, tag + ("-" + label if label != "first" else ""), run, t_nav)
                frames = pg.evaluate("window.__fr || []")
                lts = pg.evaluate("window.__lt || []")
                info = pg.evaluate("""() => { const c = document.createElement('canvas'); let r = '';
                    try { const g = c.getContext('webgl2'); const e = g.getExtension('WEBGL_debug_renderer_info'); r = e ? g.getParameter(e.UNMASKED_RENDERER_WEBGL) : ''; } catch (e) {}
                    return {ua: navigator.userAgent, url: location.href, dpr: devicePixelRatio, gpuRenderer: r, hasGpu: 'gpu' in navigator}; }""")
                ph = {k: stats(frames, v[0], v[1]) for k, v in marks.items() if k != "boot"}
                ph["all-steady"] = stats([f for f in frames if any(marks[s][0] < f[0] <= marks[s][1] for s in STEADY if s in marks)], 0, 1e12)
                lt_by = {k: [round(d) for (t, d) in lts if v[0] < t <= v[1] and d >= 50] for k, v in marks.items() if k != "boot"}
                print("RUN %d %s [%s, cpu %gx, %s] title-up %.1f s | url %s" % (run, label, a.scenario, a.cpu, tag, up or -1, info["url"]))
                print("  %-13s %6s %7s %7s %7s %8s %9s  worst5" % ("phase", "n", "p50", "p95", "p99", "worst", ">33ms"))
                for k, s in ph.items():
                    print("  %-13s %6d %7.1f %7.1f %7.1f %8.1f %4d %4.1f%%  %s" % (k, s["n"], s["p50"], s["p95"], s["p99"], s["worst"], s["over33"], s["over33_pct"], s["worst5"]))
                results[label] = {"title_up_s": up, "phases": ph, "marks": marks, "longtasks_ge50ms": lt_by, "info": info,
                                  "frames": [[round(t, 1), round(d, 1)] for (t, d) in frames],
                                  "console": [[round(t - t_nav, 2), s] for t, s in con]}
                if a.pass_bar:
                    bar = bar_for(a.cpu)
                    for ph_name in STEADY:
                        s = ph.get(ph_name)
                        if not s or s["n"] < 30:
                            print("  BAR: %s has no frames (%s)" % (ph_name, s)); failed = True; continue
                        miss = [k for k in ("p50", "p95", "p99", "over33_pct", "worst") if s[k] > bar[k]]
                        if miss:
                            failed = True
                            print("  BAR MISS %s: %s (bar %s)" % (ph_name, {k: s[k] for k in miss}, bar))
            json.dump(results, open(os.path.join(a.out, "%s-run%d.json" % (tag, run)), "w"))
            ctx.close()
            if not a.keep_profile:
                shutil.rmtree(prof, ignore_errors=True)
    if a.pass_bar:
        print("PASS BAR: %s" % ("FAIL" if failed else "ok"))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
