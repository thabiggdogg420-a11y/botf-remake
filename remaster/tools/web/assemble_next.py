#!/usr/bin/env python3
"""Assembles the two-build host folder (loader + webgpu/ + webgl/ + one shared packs/) from two exports. Barclay, 21 Nov 2026.

  python assemble_next.py --webgl <export dir> --webgpu <export dir> --packs <hashed packs dir> --loader <godot/web-host/loader.html>
                          --wgsl-cache <wgsl-cache.js> --hosting <HOSTING.md> --version "<text>" --out <folder>

Each export dir holds index.html + engine files from tools/web/export_web.ps1. Steps:
  1. copies each export (no packs/, no logs) to <out>/webgl and <out>/webgpu, the packs to <out>/packs;
  2. WebGPU: drops index.shaders_extra.bin (the Forward+ advanced group the game never turns on), adds wgsl-cache.js and its
     <script> tag;
  3. both pages get globalThis.BOTF_PACKS='../packs/' (the shared pack set) before the engine starts;
  4. tools/verify/hash_web_packs.py stamps the build id, copies sw.js and writes .gz/.br sidecars and sizes.json per build
     (run with a temporary packs/ link so its manifest step sees the shared packs, which are already hashed);
  5. VERSION.txt, HOSTING.md, .htaccess and nginx.conf.example (from the HOSTING.md folder; the host needs them, R8), the loader
     index.html (+ sidecars). packs/ gets no .br/.gz sidecars: nothing serves them (.htaccess and nginx never do, and
     app_loader fetches the plain .pck); R7 wrote 286 MB of them (dropped in R8, Gemini audit).
  6. Desktop | Mobile (mobile stage M-A, 25 Nov 2026): the loader is godot/web-host/loader.html, and both exports' pages carry the
     mode script (godot/web-host/botf-mode.js, embedded in the Web and WebGPU presets). The assembler refuses a loader or a page
     without it. Mobile always gets webgl/ (the loader and the webgpu/ page both send it there).
"""
import argparse
import gzip
import zlib
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
HASHER = os.path.join(HERE, "..", "verify", "hash_web_packs.py")
PACKS_TAG = "<script>globalThis.BOTF_PACKS='../packs/';</script>"
WGSL_TAG = '<script src="wgsl-cache.js"></script>'


def sidecars(path, min_gain=0.10):
    import brotli
    raw = open(path, "rb").read()
    if len(zlib.compress(raw[:4 << 20], 1)) > min(len(raw), 4 << 20) * (1 - min_gain):   # quick probe: WebP/OGG packs skip brotli q11
        return
    for ext, data in ((".gz", gzip.compress(raw, 9, mtime=0)), (".br", brotli.compress(raw, quality=11))):
        if len(data) < len(raw) * (1 - min_gain):
            open(path + ext, "wb").write(data)
        elif os.path.exists(path + ext):
            os.remove(path + ext)


def inject(html_path, tags):
    h = open(html_path, encoding="utf-8").read()
    for t in tags:
        if t not in h:
            h = h.replace("<head>", "<head>" + t, 1)
    open(html_path, "w", encoding="utf-8", newline="").write(h)


def copy_build(src, dst, packs):
    shutil.copytree(src, dst, ignore=shutil.ignore_patterns("packs", "export.log*", "VERSION.txt", "*.import", "index.shaders_extra.bin"))
    link = os.path.join(dst, "packs")
    shutil.copytree(packs, link, copy_function=os.link)   # hard links: the hasher reads the shared set, nothing is copied
    return link


def main():
    ap = argparse.ArgumentParser()
    for k in ("webgl", "webgpu", "packs", "loader", "wgsl-cache", "hosting", "version", "out"):
        ap.add_argument("--" + k, required=True)
    a = ap.parse_args()
    if os.path.exists(a.out):
        sys.exit("out folder exists (archive it first): " + a.out)
    if "data-botf-mode" not in open(a.loader, encoding="utf-8").read():
        sys.exit("the loader has no Desktop | Mobile support: use godot/web-host/loader.html (" + a.loader + ")")
    os.makedirs(a.out)
    for name, src in (("webgl", a.webgl), ("webgpu", a.webgpu)):
        dst = os.path.join(a.out, name)
        link = copy_build(src, dst, a.packs)
        html = os.path.join(dst, "index.html")
        tags = [PACKS_TAG]
        if name == "webgpu":
            shutil.copy2(a.wgsl_cache, os.path.join(dst, "wgsl-cache.js"))
            tags.append(WGSL_TAG)
        inject(html, tags)
        if "data-botf-mode" not in open(html, encoding="utf-8").read():
            sys.exit(name + "/index.html has no Desktop | Mobile switch (export it from a preset that carries botf-mode.js: tools/web/embed_mode_js.py)")
        r = subprocess.run([sys.executable, HASHER, dst, "--first-budget-mb", "999", "--total-budget-mb", "9999"], capture_output=True, text=True)
        print(name, r.stdout.strip().splitlines()[-3:] if r.stdout else "", r.stderr.strip()[-400:])
        if r.returncode != 0:
            sys.exit("hash_web_packs failed for " + name)
        man = os.path.join(link, "manifest.json")
        if name == "webgl":
            shutil.copy2(man, os.path.join(a.out, "manifest.tmp"))
        shutil.rmtree(link)
        for f in ("wgsl-cache.js", "index.html", "index.shaders.pck"):
            p = os.path.join(dst, f)
            if os.path.exists(p):
                sidecars(p)
    shutil.copytree(a.packs, os.path.join(a.out, "packs"), ignore=shutil.ignore_patterns("*.gz", "*.br"))
    os.replace(os.path.join(a.out, "manifest.tmp"), os.path.join(a.out, "packs", "manifest.json"))
    shutil.copy2(a.loader, os.path.join(a.out, "index.html"))
    sidecars(os.path.join(a.out, "index.html"))
    shutil.copy2(a.hosting, os.path.join(a.out, "HOSTING.md"))
    host_dir = os.path.dirname(os.path.abspath(a.hosting))
    for f in (".htaccess", "nginx.conf.example"):
        src = os.path.join(host_dir, f)
        if not os.path.isfile(src):
            sys.exit("missing host file beside HOSTING.md: " + src)
        shutil.copy2(src, os.path.join(a.out, f))
    open(os.path.join(a.out, "VERSION.txt"), "w", encoding="utf-8").write(a.version.replace("\\n", "\n") + "\n")
    print("assembled", a.out)


if __name__ == "__main__":
    main()
