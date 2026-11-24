#!/usr/bin/env python3
"""One line per pass for the perf_frames.py json files in a folder (25 Nov 2026, Barclay).

  python perf_table.py <dir> [<glob>=*.json] [--md]

Per file and pass (first / reload): boot time to "BOTF title up", then for the title (idle + mouse movement) and the galaxy
(idle + mouse movement): frames, p50, p95, p99, worst and the share of frames over 33 ms.
"""
import glob
import json
import os
import sys


def pct(v, q):
    s = sorted(v)
    return s[min(len(s) - 1, int(round(q * (len(s) - 1))))] if s else 0.0


def seg(frames, marks, names):
    out = []
    for n in names:
        if n in marks:
            t0, t1 = marks[n]
            out += [d for t, d in frames if t0 < t <= t1]
    return out


def row(v):
    if not v:
        return "no frames"
    over = sum(1 for d in v if d > 33.4)
    return "%5d  p50 %5.1f  p95 %6.1f  p99 %6.1f  worst %6.0f  >33ms %5.1f%%" % (len(v), pct(v, .5), pct(v, .95), pct(v, .99), max(v), 100.0 * over / len(v))


def main():
    d = sys.argv[1]
    pat = sys.argv[2] if len(sys.argv) > 2 and not sys.argv[2].startswith("--") else "*.json"
    for f in sorted(glob.glob(os.path.join(d, pat))):
        try:
            j = json.load(open(f))
        except Exception:
            continue
        for label, r in j.items():
            if not isinstance(r, dict) or "frames" not in r:
                continue
            print("%s [%s] title-up %.1f s" % (os.path.basename(f).replace("-run1.json", ""), label, r.get("title_up_s") or -1))
            print("    title : " + row(seg(r["frames"], r["marks"], ["title-idle", "title-move"])))
            print("    galaxy: " + row(seg(r["frames"], r["marks"], ["galaxy-idle", "galaxy-move"])))


if __name__ == "__main__":
    main()
