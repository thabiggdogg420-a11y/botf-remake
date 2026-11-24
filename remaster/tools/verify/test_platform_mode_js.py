#!/usr/bin/env python3
"""Desktop | Mobile mode detection in the host page (godot/web-host/botf-mode.js), run under node with a fake browser.

  python remaster/tools/verify/test_platform_mode_js.py

Checks: every case in godot/fixtures/platform-mode-cases.json (the file the game's PlatformMode.decide() is tested against too:
query string, remembered choice, touch + narrow-screen auto-detect); a localStorage that throws; the switch (stores the choice,
clears the remembered build, goes to the loader root, keeps ?mode= when storage is refused); Mobile leaving a webgpu/ page for
webgl/; the loader's rule (Mobile always WebGL, ?mode= forwarded); and that the copies in export_presets.cfg and loader.html
match the source. Exit code 0 on success. Barclay, 25 Nov 2026 (mobile stage M-A)."""
import json
import os
import re
import subprocess
import sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
GODOT = os.path.join(ROOT, "godot")
CASES = json.load(open(os.path.join(GODOT, "fixtures", "platform-mode-cases.json"), encoding="utf-8"))["cases"]
SRC = os.path.join(GODOT, "web-host", "botf-mode.js")

HARNESS = r"""
const vm = require('vm'), fs = require('fs');
const src = fs.readFileSync(process.argv[1], 'utf8');
const c = JSON.parse(process.argv[2]);
function el() { return { children: [], style: {}, dataset: {}, listeners: {}, className: '', attrs: {},
  appendChild(x) { this.children.push(x); return x; }, setAttribute(k, v) { this.attrs[k] = v; },
  addEventListener(t, f) { this.listeners[t] = f; }, set textContent(v) { this._t = v; }, get textContent() { return this._t; } }; }
const store = {}; let storeThrows = !!c.storeThrows;
const calls = { href: null, replaced: null, removed: [] };
const ctx = { console, URL, URLSearchParams, Math, String, globalThis: null };
ctx.window = ctx; ctx.globalThis = ctx;
ctx.screen = { width: c.short ? c.short + 400 : 0, height: c.short };
ctx.navigator = { maxTouchPoints: c.touch ? 5 : 0 };
if (c.touch) ctx.ontouchstart = null;
ctx.location = { search: c.query ? '?mode=' + c.query : '', pathname: c.path || '/botf/', hash: '', origin: 'https://x.test',
  get href() { return 'https://x.test' + this.pathname + this.search; }, set href(v) { calls.href = v; }, replace(v) { calls.replaced = v; } };
ctx.localStorage = { getItem(k) { if (storeThrows) throw new Error('denied'); return k in store ? store[k] : null; },
  setItem(k, v) { if (storeThrows) throw new Error('denied'); store[k] = v; } };
if (c.stored) store['botf-mode'] = c.stored;
ctx.sessionStorage = { removeItem(k) { calls.removed.push(k); }, getItem() { return null; }, setItem() {} };
const body = el(), head = el(); const byId = {};
ctx.document = { readyState: 'complete', head, body, createElement() { return el(); },
  getElementById(id) { return byId[id] || null; }, addEventListener() {} };
const origAppend = body.appendChild.bind(body); body.appendChild = (x) => { if (x.id) byId[x.id] = x; return origAppend(x); };
vm.createContext(ctx);
vm.runInContext(src, ctx);
const out = { mode: ctx.BOTF_MODE, source: ctx.BOTF_MODE_SRC, replaced: calls.replaced };
if (c.switchTo) { ctx.botfSetMode(c.switchTo); out.href = calls.href; out.stored = store['botf-mode'] || null; out.removed = calls.removed; }
const box = byId['botf-mode']; out.buttons = box ? box.children.map(b => [b.dataset.mode, b.className]) : null;
console.log(JSON.stringify(out));
"""


def run(case: dict, src: str = SRC) -> dict:
    r = subprocess.run(["node", "-e", HARNESS, src, json.dumps(case)], capture_output=True, text=True, timeout=30)
    if r.returncode != 0:
        raise RuntimeError(r.stderr[-800:])
    return json.loads(r.stdout.strip().splitlines()[-1])


fails = 0
checks = 0


def check(cond: bool, what: str) -> None:
    global fails, checks
    checks += 1
    if cond:
        print("  ok  ", what)
    else:
        fails += 1
        print("  FAIL", what)


for c in CASES:
    o = run(c)
    check(o["mode"] == c["mode"] and o["source"] == c["source"], "%s -> %s (%s)" % (c["name"], o["mode"], o["source"]))

o = run({"query": "", "stored": "mobile", "touch": False, "short": 1080, "storeThrows": True})
check(o["mode"] == "desktop" and o["source"] == "auto", "a localStorage that throws is ignored, not fatal")

o = run({"query": "", "stored": "", "touch": False, "short": 1080, "switchTo": "mobile"})
check(o["stored"] == "mobile", "the switch remembers the choice in localStorage")
check("botf-build" in o["removed"], "the switch clears the tab's remembered build so the loader decides again")
check(o["href"] == "https://x.test/botf/", "the switch reloads the page without ?mode= (the stored choice rules)")
check(o["buttons"] == [["desktop", "on"], ["mobile", ""]], "the page shows Desktop | Mobile with the current mode marked: %s" % o["buttons"])

o = run({"query": "", "stored": "", "touch": False, "short": 1080, "switchTo": "mobile", "path": "/botf/webgl/index.html"})
check(o["href"] == "https://x.test/botf/", "from webgl/ the switch goes to the loader root, which picks the build")

o = run({"query": "", "stored": "", "touch": False, "short": 1080, "switchTo": "mobile", "storeThrows": True})
check(o["href"] == "https://x.test/botf/?mode=mobile", "with storage refused the choice rides on ?mode=")

o = run({"query": "", "stored": "", "touch": True, "short": 390, "path": "/botf/webgpu/index.html"})
check(o["replaced"] == "/botf/webgl/index.html", "Mobile on a webgpu/ page is sent to webgl/: %s" % o["replaced"])
o = run({"query": "", "stored": "", "touch": False, "short": 1080, "path": "/botf/webgpu/index.html"})
check(o["replaced"] is None, "Desktop stays on webgpu/")
o = run({"query": "mobile", "stored": "", "touch": False, "short": 1080, "path": "/botf/webgpu/index.html"})
check(o["replaced"] == "/botf/webgl/index.html?mode=mobile", "?mode=mobile forces webgl/ too, query kept")

# The loader: Mobile always WebGL (even with a WebGPU-capable browser), ?mode= forwarded, Desktop unchanged.
loader = open(os.path.join(GODOT, "web-host", "loader.html"), encoding="utf-8").read()
check("window.BOTF_MODE === 'mobile') return go('webgl', 'mobile mode')" in loader, "the loader sends Mobile to WebGL before any other rule")
check(loader.index("BOTF_MODE === 'mobile'") < loader.index("forced === 'webgl' || forced === 'webgpu') return go"), "Mobile outranks a forced ?build=webgpu")
check("'?mode=' + qm" in loader, "the loader forwards ?mode= to the game page")

# The copies match the source.
r = subprocess.run([sys.executable, os.path.join(ROOT, "tools", "web", "embed_mode_js.py"), "--check"], capture_output=True, text=True)
check(r.returncode == 0, "export presets and loader carry the current botf-mode.js (%s)" % r.stdout.strip())
presets = open(os.path.join(GODOT, "export_presets.cfg"), encoding="utf-8").read()
check(presets.count("data-botf-mode") == 2, "the Web and WebGPU presets each carry it (%d)" % presets.count("data-botf-mode"))
check(len(re.findall(r"<script data-botf-mode>", loader)) == 1, "the loader carries it once")

print("%d checks, %d failed" % (checks, fails))
sys.exit(1 if fails else 0)
