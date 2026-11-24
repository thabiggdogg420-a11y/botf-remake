#!/usr/bin/env python3
"""Where a frame's time goes in the browser build (25 Nov 2026, Barclay): main-thread callback time, GL work per frame and
frame pacing, per phase, for the same title -> galaxy flow as perf_frames.py (it imports that tool's flow and stats).

  python perf_probe.py <url> <out_dir> [--cpu N] [--tag NAME] [--chrome-arg=--disable-gpu-vsync ...] [--w --h --phase-s]

On top of perf_frames' requestAnimationFrame recorder it wraps requestAnimationFrame so each callback (the game's whole main
loop step: scripts, scene update, GL command submit) is timed (cb ms), and wraps the WebGL2 draw/bind/use calls so each step
counts its draw calls, program switches, framebuffer binds and texture uploads. cb ms << frame interval = the frame waits on
the GPU or vsync; cb ms ~ frame interval = the main thread is the limit. Per phase it prints mean/p95 cb ms, draws per frame,
program switches per frame, frame interval p50/p95. Run it with --chrome-arg=--disable-gpu-vsync
--chrome-arg=--disable-frame-rate-limit to see the uncapped throughput (the cost of a frame with the pacing taken away).
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

PROBE = """
(() => {
  window.__cb = []; const P = WebGL2RenderingContext.prototype;
  let c = {draw: 0, prog: 0, fbo: 0, tex: 0, buf: 0, inst: 0, tris: 0};
  const w = (n, f) => { const o = P[n]; if (!o) return; P[n] = function(...a) { f(a); return o.apply(this, a); }; };
  // one-frame draw census: program (hash of its fragment+vertex source) and bound framebuffer, per draw; window.__census(k) returns the last k frames' aggregate
  const hs = (str) => { let x = 0; for (let i = 0; i < str.length; i++) x = (x * 31 + str.charCodeAt(i)) | 0; return (x >>> 0).toString(16); };
  const psrc = new WeakMap(); const pk = new WeakMap(); let curP = null, curF = 'screen'; window.__cen = {}; window.__cenOn = false; window.__srcs = {};
  const as = P.attachShader; P.attachShader = function(p, sh) { psrc.set(p, (psrc.get(p) || '') + (this.getShaderSource(sh) || '')); return as.call(this, p, sh); };
  const bf = P.bindFramebuffer; P.bindFramebuffer = function(t, f) { curF = f ? String(f.__id || (f.__id = ++window.__fid)) : 'screen'; return bf.call(this, t, f); }; window.__fid = 0;
  const up = P.useProgram; P.useProgram = function(p) { curP = p; return up.call(this, p); };
  const census = (n) => { if (!window.__cenOn || !curP) return; let k = pk.get(curP); if (!k) { const src = psrc.get(curP) || ''; k = hs(src); pk.set(curP, k); if (!(k in window.__srcs)) window.__srcs[k] = src.split('\\n').filter(l => l.startsWith('#define') || /^uniform .*(shader|material)/i.test(l)).slice(0, 12).join(' ').slice(0, 400) + ' len=' + src.length; }
      const key = curF + '|' + k; window.__cen[key] = (window.__cen[key] || 0) + n; };
  // synchronous queries (getParameter and friends block on the GPU process): per pname, count and time, while the census is on
  window.__gq = {};
  ['getParameter', 'getProgramParameter', 'getShaderParameter', 'getError', 'getUniformLocation', 'getActiveUniform', 'getQueryParameter', 'getBufferParameter', 'getFramebufferAttachmentParameter', 'checkFramebufferStatus', 'readPixels', 'getSupportedExtensions', 'getExtension', 'getShaderInfoLog', 'getProgramInfoLog'].forEach(n => { const o = P[n]; if (!o) return;
    P[n] = function(...a) { if (!window.__cenOn) return o.apply(this, a); const t = performance.now(); const r = o.apply(this, a); const d = performance.now() - t;
      const k = n + ':' + (typeof a[a.length - 1] === 'number' ? a[a.length - 1] : (a.length > 1 ? 'x' : '')) + (n === 'getProgramParameter' || n === 'getShaderParameter' ? '' : ''); const e = window.__gq[k] || (window.__gq[k] = [0, 0]); e[0]++; e[1] += d; return r; }; });
  window.__hist = {};
  const hist = (n, a) => { if (!window.__cenOn) return; const cnt = n.indexOf('Arrays') >= 0 ? a[2] : a[1]; const inst = n.indexOf('Instanced') >= 0 ? a[a.length - 1] : 1; const k = n + ' count=' + cnt + ' inst=' + inst + ' fbo=' + curF; window.__hist[k] = (window.__hist[k] || 0) + 1; };
  ['drawArrays', 'drawElements'].forEach(n => w(n, (a) => { c.draw++; census(1); hist(n, a); }));
  ['drawArraysInstanced', 'drawElementsInstanced'].forEach(n => w(n, (a) => { c.draw++; c.inst++; census(1); hist(n, a); }));
  w('useProgram', () => { c.prog++; }); w('bindFramebuffer', () => { c.fbo++; });
  ['texImage2D', 'texSubImage2D', 'texImage3D', 'texSubImage3D', 'compressedTexImage2D', 'compressedTexSubImage2D'].forEach(n => w(n, () => { c.tex++; }));
  ['bufferData', 'bufferSubData'].forEach(n => w(n, () => { c.buf++; }));
  // ablation: --skip '<js expr over fbo, prog>' drops matching draw calls (the frame's other work stays), to see what a pass costs
  const pkey = (p) => { if (!p) return ''; let k = pk.get(p); if (!k) { k = hs(psrc.get(p) || ''); pk.set(p, k); } return k; };
  window.__skip = null;
  ['drawArrays', 'drawElements', 'drawArraysInstanced', 'drawElementsInstanced'].forEach(n => { const o = P[n]; P[n] = function(...a) { if (window.__skip && window.__skip(curF, pkey(curP))) return; return o.apply(this, a); }; });
  // GPU time per frame: EXT_disjoint_timer_query_webgl2 around the whole callback (all the frame's GL work), read back later
  let gl = null, ext = null; const pend = []; window.__gpu = [];
  const gc = HTMLCanvasElement.prototype.getContext;
  HTMLCanvasElement.prototype.getContext = function(k, ...r) { const x = gc.call(this, k, ...r); if (x && k === 'webgl2' && !gl) { gl = x; try { ext = x.getExtension('EXT_disjoint_timer_query_webgl2'); } catch (e) {} window.__hasTimer = !!ext; } return x; };
  const poll = () => { while (pend.length) { const [q, t] = pend[0]; if (!gl.getQueryParameter(q, gl.QUERY_RESULT_AVAILABLE)) break; pend.shift();
      if (!gl.getParameter(ext.GPU_DISJOINT_EXT)) window.__gpu.push([t, gl.getQueryParameter(q, gl.QUERY_RESULT) / 1e6]); gl.deleteQuery(q); } };
  const raf = window.requestAnimationFrame.bind(window);
  window.requestAnimationFrame = (cb) => raf((t) => {
    let q = null; if (ext && pend.length < 8) { try { poll(); q = gl.createQuery(); gl.beginQuery(ext.TIME_ELAPSED_EXT, q); } catch (e) { q = null; } }
    const s = performance.now(); c = {draw: 0, prog: 0, fbo: 0, tex: 0, buf: 0, inst: 0, tris: 0};
    try { cb(t); } finally { window.__cb.push([t, performance.now() - s, c.draw, c.prog, c.fbo, c.tex, c.buf]);
      if (q) { try { gl.endQuery(ext.TIME_ELAPSED_EXT); pend.push([q, t]); } catch (e) {} } }
  });
})();
"""


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("url")
    ap.add_argument("out")
    ap.add_argument("--cpu", type=float, default=1.0)
    ap.add_argument("--tag", default="probe")
    ap.add_argument("--w", type=int, default=1280)
    ap.add_argument("--h", type=int, default=720)
    ap.add_argument("--phase-s", type=float, default=6.0)
    ap.add_argument("--chrome-arg", action="append", default=[])
    ap.add_argument("--preload", action="store_true")
    ap.add_argument("--ablate", action="store_true", help="after the flow, 5 s windows with groups of draws dropped (3D viewport, bloom chain, root canvas)")
    ap.add_argument("--ablate-title", action="store_true", help="only the title: 5 s windows with the canvas (fbo 2) or the 3D passes dropped")
    ap.add_argument("--skip", default="", help="JS expression over (fbo, prog): draws it matches are dropped from the galaxy-idle phase on")
    ap.add_argument("--census", action="store_true", help="count draws per (framebuffer, program) over the whole run's galaxy-idle phase")
    ap.add_argument("--keep-profile", default="")
    a = ap.parse_args()
    a.scenario = "cold"
    os.makedirs(a.out, exist_ok=True)
    from playwright.sync_api import sync_playwright
    with sync_playwright() as p:
        prof = a.keep_profile or tempfile.mkdtemp(prefix="botf-probe-")
        args = ["--use-angle=d3d11", "--enable-unsafe-webgpu", "--ignore-gpu-blocklist", "--mute-audio",
                "--window-position=-10000,-10000"] + a.chrome_arg
        ctx = p.chromium.launch_persistent_context(prof, channel="chrome", headless=False, args=args, viewport={"width": a.w, "height": a.h})
        pf.lower()
        ctx.add_init_script(pf.REC)
        ctx.add_init_script(PROBE)
        pg = ctx.pages[0] if ctx.pages else ctx.new_page()
        cdp = ctx.new_cdp_session(pg)
        if a.cpu > 1:
            cdp.send("Emulation.setCPUThrottlingRate", {"rate": a.cpu})
        con = []
        pg.on("console", lambda m: con.append([time.time(), m.text]))
        t_nav = time.time()
        pg.goto(a.url, wait_until="load")
        if a.ablate_title:
            while time.time() - t_nav < 240 * max(1.0, a.cpu) and not any("BOTF title up" in c[1] for c in con):
                pg.wait_for_timeout(250)
            pg.wait_for_timeout(int(12000 * max(1.0, a.cpu)))
            for nm, ex in [("none", "false"), ("no-canvas(fbo2)", "fbo === '2'"), ("no-3d(fbo!=2)", "fbo !== '2'"), ("nothing", "true"), ("none-again", "false")]:
                pg.evaluate("window.__skip = (fbo, prog) => (%s)" % ex)
                t0 = pg.evaluate("performance.now()"); pg.wait_for_timeout(5000); t1 = pg.evaluate("performance.now()")
                fr = [d for (t, d) in pg.evaluate("window.__fr") if t0 + 500 < t <= t1]
                cb = [c[1] for c in pg.evaluate("window.__cb") if t0 + 500 < c[0] <= t1]
                print("  TITLE-ABLATE %-18s n=%4d dt p50 %5.1f p95 %5.1f | cb avg %5.1f" % (nm, len(fr), pf.pct(fr, .5), pf.pct(fr, .95), sum(cb) / max(1, len(cb))))
            ctx.close(); return
        marks, up = pf.run_flow(pg, a, con, a.tag, 1, t_nav)
        if a.ablate:
            wins = [("none", "false"), ("no-3d(fbo48)", "fbo === '48'"), ("no-bloom/glow(fbo!=2,48)", "fbo !== '2' && fbo !== '48' && fbo !== 'screen' && fbo !== '1'"),
                    ("no-root-canvas(fbo2)", "fbo === '2'"), ("no-draws-at-all", "true"), ("none-again", "false")]
            ab = {}
            for nm, ex in wins:
                pg.evaluate("window.__skip = (fbo, prog) => (%s)" % ex)
                t0 = pg.evaluate("performance.now()")
                pg.wait_for_timeout(5000)
                t1 = pg.evaluate("performance.now()")
                fr = [d for (t, d) in pg.evaluate("window.__fr") if t0 + 500 < t <= t1]
                cb = [c[1] for c in pg.evaluate("window.__cb") if t0 + 500 < c[0] <= t1]
                ab[nm] = {"dt_p50": pf.pct(fr, .5), "dt_p95": pf.pct(fr, .95), "cb_avg": sum(cb) / max(1, len(cb)), "n": len(fr)}
                print("  ABLATE %-28s n=%4d dt p50 %5.1f p95 %5.1f | cb avg %5.1f" % (nm, len(fr), ab[nm]["dt_p50"], ab[nm]["dt_p95"], ab[nm]["cb_avg"]))
            pg.evaluate("window.__skip = null")
        frames = pg.evaluate("window.__fr || []")
        cbs = pg.evaluate("window.__cb || []")
        gpu = pg.evaluate("window.__gpu || []")
        print("  GPU timer extension:", pg.evaluate("window.__hasTimer"), "samples", len(gpu))
        if getattr(a, "cen_out", None):
            json.dump(a.cen_out, open(os.path.join(a.out, a.tag + "-census.json"), "w"), indent=1)
        print("PROBE %s cpu %gx title-up %.1f s" % (a.tag, a.cpu, up or -1))
        print("  %-13s %6s | %6s %6s %6s | %6s %6s %5s %5s %5s | %6s %6s | %6s %6s" % ("phase", "frames", "cb avg", "cb p95", "cb max", "draws", "progs", "fbos", "tex", "buf", "dt p50", "dt p95", "gpu avg", "gpu p95"))
        out = {}
        for k, (t0, t1) in marks.items():
            if k == "boot":
                continue
            seg = [c for c in cbs if t0 < c[0] <= t1]
            fr = [d for (t, d) in frames if t0 < t <= t1]
            if not seg:
                continue
            n = len(seg)
            cb = [c[1] for c in seg]
            row = {"n": n, "cb_avg": sum(cb) / n, "cb_p95": pf.pct(cb, .95), "cb_max": max(cb), "draws": sum(c[2] for c in seg) / n,
                   "progs": sum(c[3] for c in seg) / n, "fbos": sum(c[4] for c in seg) / n, "tex": sum(c[5] for c in seg) / n,
                   "buf": sum(c[6] for c in seg) / n, "dt_p50": pf.pct(fr, .5), "dt_p95": pf.pct(fr, .95)}
            gp = [g[1] for g in gpu if t0 < g[0] <= t1]
            row["gpu_avg"] = sum(gp) / len(gp) if gp else -1
            row["gpu_p95"] = pf.pct(gp, .95) if gp else -1
            out[k] = row
            print("  %-13s %6d | %6.1f %6.1f %6.1f | %6.1f %6.1f %5.1f %5.2f %5.1f | %6.1f %6.1f | %6.1f %6.1f" % (
                k, n, row["cb_avg"], row["cb_p95"], row["cb_max"], row["draws"], row["progs"], row["fbos"], row["tex"], row["buf"], row["dt_p50"], row["dt_p95"], row["gpu_avg"], row["gpu_p95"]))
        json.dump({"phases": out, "console": [[round(t - t_nav, 2), s] for t, s in con]}, open(os.path.join(a.out, a.tag + ".json"), "w"))
        ctx.close()
        if not a.keep_profile:
            shutil.rmtree(prof, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
