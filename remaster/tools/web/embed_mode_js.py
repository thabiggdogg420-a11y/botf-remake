#!/usr/bin/env python3
"""Copies godot/web-host/botf-mode.js (the Desktop | Mobile rule and switch), minified to one line, into the three places that
run it before the game: the head_include of the "Web" and "WebGPU" export presets, and the loader page (web-host/loader.html).

  python embed_mode_js.py            rewrite the copies
  python embed_mode_js.py --check    exit 1 when a copy differs from the source (the unit test runs this)

A copy is  <script data-botf-mode>...</script>.  In the loader it sits between  <!--botf-mode-->  and  <!--/botf-mode-->.
In the presets it follows the loading-screen script (so it can wrap botfBootDone) and is escaped as a cfg string.
Barclay, 25 Nov 2026 (mobile stage M-A).
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
GODOT = os.path.normpath(os.path.join(HERE, "..", "..", "godot"))
SRC = os.path.join(GODOT, "web-host", "botf-mode.js")
PRESETS = os.path.join(GODOT, "export_presets.cfg")
LOADER = os.path.join(GODOT, "web-host", "loader.html")
BOOT_END = "setTimeout(function(){globalThis.botfBootDone();},240000);})();</script>"
PRESET_NAMES = ("Web", "WebGPU")


def minified() -> str:
    js = open(SRC, encoding="utf-8").read()
    js = re.sub(r"^\s*/\*.*?\*/", "", js, count=1, flags=re.S)   # the header comment
    lines = [ln.strip() for ln in js.splitlines() if ln.strip()]
    assert not any("//" in ln.replace("://", "") for ln in lines), "no // comments in botf-mode.js (the copy is one line)"
    return " ".join(lines)


def tag() -> str:
    return "<script data-botf-mode>" + minified() + "</script>"


def cfg_escape(s: str) -> str:
    return s.replace("\\", "\\\\").replace('"', '\\"')


TAG_RE = re.compile(r"<script data-botf-mode>.*?</script>", re.S)


def patch_presets(text: str) -> str:
    out = []
    # a preset block starts at "[preset.N]" and its options follow at "[preset.N.options]"
    parts = re.split(r"(?m)^(?=\[preset\.\d+\]\r?$)", text)
    for part in parts:
        m = re.search(r'(?m)^name="([^"]+)"', part)
        if m and m.group(1) in PRESET_NAMES:
            esc = cfg_escape(tag())
            head = re.search(r'(?m)^html/head_include="(.*)"\r?$', part)
            assert head, "no head_include in preset " + m.group(1)
            body = head.group(1)
            esc_re = re.compile(re.escape(cfg_escape("<script data-botf-mode>")) + r".*?" + re.escape(cfg_escape("</script>")), re.S)
            if esc_re.search(body):
                new_body = esc_re.sub(lambda _m: esc, body, count=1)
            else:
                assert BOOT_END in body, "boot script not found in preset " + m.group(1)
                new_body = body.replace(BOOT_END, BOOT_END + esc, 1)
            part = part[:head.start(1)] + new_body + part[head.end(1):]
        out.append(part)
    return "".join(out)


def patch_loader(text: str) -> str:
    block = "<!--botf-mode-->" + tag() + "<!--/botf-mode-->"
    new, n = re.subn(r"<!--botf-mode-->.*?<!--/botf-mode-->", lambda _m: block, text, count=1, flags=re.S)
    assert n == 1, "loader.html has no <!--botf-mode--> markers"
    return new


def main() -> int:
    check = "--check" in sys.argv
    bad = []
    for path, fn in ((PRESETS, patch_presets), (LOADER, patch_loader)):
        raw = open(path, encoding="utf-8", newline="").read()
        new = fn(raw)
        if new != raw:
            if check:
                bad.append(os.path.relpath(path, GODOT))
            else:
                open(path, "w", encoding="utf-8", newline="").write(new)
                print("updated", os.path.relpath(path, GODOT))
    if check:
        if bad:
            print("DRIFT: botf-mode.js differs from its copy in: " + ", ".join(bad) + " (run tools/web/embed_mode_js.py)")
            return 1
        print("botf-mode.js copies match")
    return 0


if __name__ == "__main__":
    sys.exit(main())
