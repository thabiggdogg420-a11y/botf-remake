#!/usr/bin/env python3
"""Merges recorded wgsl-out.json files (profile_run.py DUMPWGSL=1) into <web_dir>/wgsl-cache.js and adds
<script src="wgsl-cache.js"></script> to <web_dir>/index.html <head> (idempotent). Barclay, WebGPU trial r4.
  python make_wgsl_cache.py <web_dir> <wgsl-out.json> [...]"""
import json, sys, os
d = sys.argv[1]; m = {}
for f in sys.argv[2:]:
    m.update(json.load(open(f)))
js = "globalThis.godotWgslCacheIn=" + json.dumps(m, separators=(",", ":")) + ";\n"
open(os.path.join(d, "wgsl-cache.js"), "w", encoding="utf-8").write(js)
p = os.path.join(d, "index.html"); h = open(p, encoding="utf-8").read()
tag = '<script src="wgsl-cache.js"></script>'
if tag not in h:
    h = h.replace("<head>", "<head>" + tag, 1); open(p, "w", encoding="utf-8").write(h)
print(len(m), "stages,", round(len(js) / 1e6, 2), "MB")
