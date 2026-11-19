#!/usr/bin/env python3
"""srtgrep.py <srt> <pattern> [ctx]: subtitle cues matching a regex, with ctx cues either side (offline helper for MOD-9 cut lists)."""
import re, sys
def cues(path):
    t = open(path, encoding="utf-8-sig", errors="replace").read().replace("\r", "")
    for b in re.split(r"\n\s*\n", t):
        ls = b.strip().split("\n")
        if len(ls) >= 3 and "-->" in ls[1]:
            yield ls[1].replace(",", "."), " ".join(ls[2:])
if __name__ == "__main__":
    cs = list(cues(sys.argv[1])); ctx = int(sys.argv[3]) if len(sys.argv) > 3 else 2
    for i, (tm, tx) in enumerate(cs):
        if re.search(sys.argv[2], tx, re.I):
            for j in range(max(0, i - ctx), min(len(cs), i + ctx + 1)):
                print(("> " if j == i else "  ") + cs[j][0], cs[j][1])
            print("--")
