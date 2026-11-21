#!/usr/bin/env python3
"""Cold-profile frame times of the web build's title and first galaxy open, in Firefox or Chrome (21 Nov 2026, Barclay).

  python browser_frames.py <firefox|chrome> <url> <out_dir> [--runs N] [--idle S] [--w 1280 --h 720]

Every run is a fresh browser profile (empty HTTP cache, Cache API and shader cache: Firefox keeps no GL program cache across
profiles anyway). The page gets a requestAnimationFrame recorder before any script runs. The run waits for the game's
"BOTF title up" console line, lets the title idle for --idle seconds (the title-idle warm-ups run then), clicks the Tactical
drill (--click, default 216,598 at 1280x720: a ready-made campaign that opens on the galaxy map) and records 15 s more. Per run it prints
  RUN <n> title-up <s> | idle worst <ms> (frames > 250 ms: k) | galaxy-open worst <ms> (frames > 250 ms: k)
and writes run<n>.json (frame list, console) and run<n>-galaxy.png. Firefox and Chromium come from Playwright's own builds
(Chrome: the installed channel, ANGLE D3D11, WebGPU on). Windows open off-screen; the browser runs at BelowNormal priority.
"""
import argparse
import json
import os
import sys
import time

REC = """
(() => {
  window.__fr = []; let last = performance.now();
  const tick = (t) => { window.__fr.push([t, t - last]); last = t; requestAnimationFrame(tick); };
  requestAnimationFrame(tick);
})();
"""

# --gltrace: time spent inside WebGL calls that can block (program link and its status query, shader compile, texture and
# buffer uploads), per call kind, with each linked program's source hash and the time it took, so a long frame can be
# split into shader compiles and uploads.
GLTRACE = """
(() => {
  window.__gl = []; const P = WebGL2RenderingContext.prototype;
  const h = (s) => { let x = 0; for (let i = 0; i < s.length; i++) x = (x * 31 + s.charCodeAt(i)) | 0; return (x >>> 0).toString(16); };
  const progSrc = new WeakMap();
  const wrap = (name, tag) => { const f = P[name]; if (!f) return; P[name] = function(...a) {
      const t = performance.now(); const r = f.apply(this, a); const d = performance.now() - t;
      if (d > 2) window.__gl.push([t, name, d, tag ? tag(this, a) : '']); return r; }; };
  const as = P.attachShader;
  P.attachShader = function(p, sh) { const src = this.getShaderSource(sh) || ''; progSrc.set(p, (progSrc.get(p) || '') + src); return as.call(this, p, sh); };
  window.__src = {};
  const key = (p) => { const src = progSrc.get(p) || ''; const k = h(src);
      if (!(k in window.__src)) window.__src[k] = src.split('\\n').filter(l => l.startsWith('#define')).join(' ').slice(0, 3000) + ' || ' + src.length;
      return k; };
  wrap('linkProgram', (gl, a) => key(a[0]));
  wrap('getProgramParameter', (gl, a) => key(a[0]));
  wrap('compileShader'); wrap('getShaderParameter');
  ['texImage2D', 'texSubImage2D', 'compressedTexImage2D', 'compressedTexSubImage2D', 'texImage3D', 'texSubImage3D', 'texStorage2D', 'bufferData', 'bufferSubData', 'readPixels', 'getError'].forEach(n => wrap(n));
})();
"""


def lower(pid_hint=None):
    try:
        import psutil
        for p in psutil.process_iter(["name"]):
            n = (p.info["name"] or "").lower()
            if n.startswith("firefox") or n.startswith("chrome"):
                try:
                    p.nice(psutil.BELOW_NORMAL_PRIORITY_CLASS)
                except Exception:
                    pass
    except Exception:
        pass


def window_stats(frames, t0, t1):
    seg = [d for (t, d) in frames if t0 < t <= t1]
    worst = max(seg) if seg else 0.0
    return worst, sum(1 for d in seg if d > 250), len(seg)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("browser", choices=["firefox", "chrome"])
    ap.add_argument("url")
    ap.add_argument("out")
    ap.add_argument("--runs", type=int, default=3)
    ap.add_argument("--idle", type=float, default=20.0)
    ap.add_argument("--w", type=int, default=1280)
    ap.add_argument("--h", type=int, default=720)
    ap.add_argument("--gltrace", action="store_true", help="time the blocking WebGL calls (see GLTRACE)")
    ap.add_argument("--dump-wgsl", action="store_true", help="WebGPU fork: save globalThis.godotWgslCacheOut (the WGSL the run translated) to run<n>-wgsl-out.json, for tools/webgpu-fork/make_wgsl_cache.py")
    ap.add_argument("--click", default="216,598", help="the Tactical drill button at 1280x720 (x,y)")
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    from playwright.sync_api import sync_playwright
    with sync_playwright() as p:
        for run in range(1, a.runs + 1):
            if a.browser == "firefox":
                b = p.firefox.launch(headless=False, args=["-width", str(a.w), "-height", str(a.h)],
                                     firefox_user_prefs={"media.volume_scale": "0.0", "webgl.force-enabled": True})
            else:
                b = p.chromium.launch(channel="chrome", headless=False,
                                      args=["--window-position=-10000,-10000", "--use-angle=d3d11", "--enable-unsafe-webgpu",
                                            "--ignore-gpu-blocklist", "--mute-audio"])
            lower()
            ctx = b.new_context(viewport={"width": a.w, "height": a.h})
            ctx.add_init_script(REC)
            if a.gltrace:
                ctx.add_init_script(GLTRACE)
            pg = ctx.new_page()
            if a.browser == "firefox":
                try:   # park the window off-screen (Juggler has no position switch)
                    pg.evaluate("window.moveTo(-10000, -10000)")
                except Exception:
                    pass
            con = []
            pg.on("console", lambda m: con.append([time.time(), m.text]))
            t_start = time.time()
            pg.goto(a.url, wait_until="load")
            up = None
            while time.time() - t_start < 240:
                if any("BOTF title up" in c[1] for c in con):
                    up = time.time() - t_start
                    break
                pg.wait_for_timeout(250)   # sync Playwright delivers console events only inside its own calls
            if up is None:
                print("RUN %d title never came up (see run%d.json)" % (run, run))
            t_idle0 = pg.evaluate("performance.now()")
            pg.wait_for_timeout(int(a.idle * 1000))
            pg.screenshot(path=os.path.join(a.out, "run%d-title.png" % run))
            t_click = pg.evaluate("performance.now()")
            cx, cy = (float(v) for v in a.click.split(","))
            pg.mouse.click(cx * a.w / 1280, cy * a.h / 720)
            pg.wait_for_timeout(15000)
            t_end = pg.evaluate("performance.now()")
            frames = pg.evaluate("window.__fr")
            gl = pg.evaluate("window.__gl || []") if a.gltrace else []
            src = pg.evaluate("window.__src || {}") if a.gltrace else {}
            pg.screenshot(path=os.path.join(a.out, "run%d-galaxy.png" % run))
            if a.dump_wgsl:
                wo = pg.evaluate("globalThis.godotWgslCacheOut || {}")
                hits = pg.evaluate("globalThis.godotWgslCacheInHits | 0")
                json.dump(wo, open(os.path.join(a.out, "run%d-wgsl-out.json" % run), "w"))
                print("  WGSL translated %d stages, cache hits %d" % (len(wo), hits))
            iw, ik, _ = window_stats(frames, t_idle0, t_click)
            gw, gk, gn = window_stats(frames, t_click, t_end)
            print("RUN %d title-up %.1f s | idle worst %.0f ms (frames > 250 ms: %d) | galaxy-open worst %.0f ms (frames > 250 ms: %d, %d frames)"
                  % (run, up or -1, iw, ik, gw, gk, gn))
            long_frames = [[round(t - t_click), round(d)] for (t, d) in frames if t > t_idle0 and d > 100]
            if gl:
                def split(t0, t1):
                    agg = {}
                    for (t, n, d, tag) in gl:
                        if t0 < t <= t1:
                            agg.setdefault(n, [0, 0.0])
                            agg[n][0] += 1
                            agg[n][1] += d
                    return {k: [v[0], round(v[1])] for k, v in agg.items()}
                print("  GL idle  :", split(t_idle0, t_click))
                print("  GL galaxy:", split(t_click, t_end))
            json.dump({"title_up_s": up, "gl_sources": src, "gl_calls": [[round(t - t_click), n, round(d, 1), tag] for (t, n, d, tag) in gl], "idle_worst_ms": iw, "galaxy_worst_ms": gw, "long_frames_ms_from_click": long_frames,
                       "console": [[round(t - t_start, 2), s] for t, s in con]}, open(os.path.join(a.out, "run%d.json" % run), "w"), indent=1)
            b.close()


if __name__ == "__main__":
    sys.exit(main())
