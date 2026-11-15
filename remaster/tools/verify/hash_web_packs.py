#!/usr/bin/env python3
"""Makes the web export cacheable and measures its download (M1 webparity). Run after the export and the pack copy:

  python hash_web_packs.py <web_dir> [--first-budget-mb 25] [--total-budget-mb 400]

1. Every packs/<stem>.pck is renamed to packs/<stem>.<12 hex of its sha256>.pck and packs/manifest.json maps the logical
   name to it ({"files": {...}, "bytes": {...}}). The game (AppLoader.fetch_web_pack) reads the manifest, so a hashed name
   always means the same bytes: serve_web.py lets the browser keep those files for a year, and a pack already in the
   player's IndexedDB is mounted without any request on a return visit.
2. A .gz sidecar is written beside each file that gzip shrinks by more than 10 % (the engine wasm, index.js, index.pck,
   the mesh-heavy ship packs). serve_web.py sends it with Content-Encoding: gzip when the browser accepts gzip, which is
   every browser; any static host can do the same.
   A .br sidecar (brotli q11, needs `pip install brotli`) is written the same way; a host that can serve precompressed brotli
   sends the smaller one (HOSTING.md).
3. sizes.json and the printout give the first playable load (the engine, the main pck, the page: what has to arrive
   before the title shows) and the total (every pack), both raw and as transferred. Exit 1 when a budget is exceeded.
"""
import argparse
import gzip
import hashlib
import json
import os
import re
import sys

HASHED = re.compile(r"\.[0-9a-f]{12}\.pck$")
FIRST = ["index.html", "index.js", "index.wasm", "index.side.wasm", "index.pck", "index.audio.worklet.js",
         "index.audio.position.worklet.js"]


def brotli_sidecar(path):
    """Writes path.br (quality 11) when the brotli module is present and the file shrinks by more than 10 %. Returns its size or 0."""
    try:
        import brotli
    except ImportError:
        return 0
    br = path + ".br"
    if os.path.isfile(br) and os.path.getmtime(br) >= os.path.getmtime(path):
        return os.path.getsize(br)
    data = open(path, "rb").read()
    out = brotli.compress(data, quality=11, lgwin=24)
    if len(out) > 0.9 * len(data):
        if os.path.isfile(br):
            os.remove(br)
        return 0
    open(br, "wb").write(out)
    return len(out)


def sidecar(path):
    gz = path + ".gz"
    if os.path.isfile(gz) and os.path.getmtime(gz) >= os.path.getmtime(path):
        return os.path.getsize(gz)
    with open(path, "rb") as f:
        sample = f.read(8 << 20)
    if len(gzip.compress(sample, 6)) > 0.9 * len(sample):
        if os.path.isfile(gz):
            os.remove(gz)
        return os.path.getsize(path)
    with open(path, "rb") as f, gzip.open(gz, "wb", compresslevel=9) as g:
        while True:
            b = f.read(8 << 20)
            if not b:
                break
            g.write(b)
    if os.path.getsize(gz) > 0.9 * os.path.getsize(path):
        os.remove(gz)
        return os.path.getsize(path)
    return os.path.getsize(gz)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("web")
    ap.add_argument("--first-budget-mb", type=float, default=25)
    ap.add_argument("--total-budget-mb", type=float, default=400)
    a = ap.parse_args()
    packs = os.path.join(a.web, "packs")
    os.makedirs(packs, exist_ok=True)
    files, sizes = {}, {}
    for f in sorted(os.listdir(packs)):
        p = os.path.join(packs, f)
        if not f.endswith(".pck") or HASHED.search(f):
            continue
        h = hashlib.sha256()
        with open(p, "rb") as fh:
            for b in iter(lambda: fh.read(8 << 20), b""):
                h.update(b)
        stem = f[:-4]
        name = "%s.%s.pck" % (stem, h.hexdigest()[:12])
        for old in os.listdir(packs):   # an older hashed copy of the same pack (build output, regenerated every run)
            if old.startswith(stem + ".") and HASHED.search(old.replace(".gz", "")) and old != name and old != name + ".gz":
                os.remove(os.path.join(packs, old))
        if os.path.isfile(os.path.join(packs, name)):
            os.remove(p)
        else:
            os.replace(p, os.path.join(packs, name))
    for f in sorted(os.listdir(packs)):
        if HASHED.search(f):
            stem = f[:-17]
            files[stem + ".pck"] = f
            sizes[f] = os.path.getsize(os.path.join(packs, f))
    json.dump({"files": files, "bytes": sizes}, open(os.path.join(packs, "manifest.json"), "w"), indent=1)

    gdext = [f for f in os.listdir(a.web) if f.endswith(".wasm") and f.startswith("lib")]
    first_raw = first_tx = first_br = 0
    rows = []
    for f in FIRST + gdext:
        p = os.path.join(a.web, f)
        if not os.path.isfile(p):
            continue
        raw, tx = os.path.getsize(p), sidecar(p)
        br = brotli_sidecar(p) or tx
        first_raw += raw
        first_tx += tx
        first_br += br
        rows.append((f, raw, tx, br))
    pack_raw = pack_tx = 0
    for f in sorted(files.values()):
        p = os.path.join(packs, f)
        raw = tx = os.path.getsize(p)   # packs are never gzipped: HTTPRequest.download_file cannot take a content-encoded body on the web
        if os.path.isfile(p + ".gz"):
            os.remove(p + ".gz")
        pack_raw += raw
        pack_tx += tx
        rows.append(("packs/" + f, raw, tx, tx))
    mb = 1048576.0
    rep = {"first_load_raw_mb": round(first_raw / mb, 1), "first_load_transfer_mb": round(first_tx / mb, 1),
           "total_raw_mb": round((first_raw + pack_raw) / mb, 1), "total_transfer_mb": round((first_tx + pack_tx) / mb, 1),
           "first_load_brotli_mb": round(first_br / mb, 1), "total_brotli_mb": round((first_br + pack_tx) / mb, 1),
           "files": [{"file": f, "raw": r, "transfer": t, "brotli": b} for f, r, t, b in rows]}
    json.dump(rep, open(os.path.join(a.web, "sizes.json"), "w"), indent=1)
    for f, r, t, b in rows:
        print("  %-48s %8.1f MB  gzip %8.1f MB  brotli %8.1f MB" % (f, r / mb, t / mb, b / mb))
    print("first playable load: %.1f MB raw, %.1f MB gzip, %.1f MB brotli (budget %.0f MB)" % (first_raw / mb, first_tx / mb, first_br / mb, a.first_budget_mb))
    print("total with every pack: %.1f MB raw, %.1f MB transferred (budget %.0f MB)" % ((first_raw + pack_raw) / mb, (first_tx + pack_tx) / mb, a.total_budget_mb))
    ok = first_tx / mb <= a.first_budget_mb and (first_tx + pack_tx) / mb <= a.total_budget_mb
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
