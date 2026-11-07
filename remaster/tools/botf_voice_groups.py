#!/usr/bin/env python3
"""Reads converse.bin from the user's own BOTF stbof.res and writes which english.snd clip belongs to which race and
conversation group (format per the Ultimate Editor's Converse.java, GPL2; re-implemented, no code copied).

  python botf_voice_groups.py --res <path to stbof.res> --out <voice-groups.json>

Output (private, local, never commit): {"clips": {"<snd index>": {"race": 0-4|36|37, "group": n, "groupType": 0|1,
"list": n|-1, "listType": n|-1}}}. Race index: 0 Cardassian, 1 Federation, 2 Ferengi, 3 Klingon, 4 Romulan, 36 alien,
37 neutral. A group is one situation (e.g. "target destroyed"); it holds one entry per race.
"""
import argparse
import json
import struct
import zipfile

V_OFFSET = 15000


def parse(data):
    n_groups, map_addr, n_lists = struct.unpack_from("<III", data, 0)
    pos = map_addr
    gmap = []
    for _ in range(n_groups):
        gtype, _z, size, addr = struct.unpack_from("<IIII", data, pos)
        pos += 16
        gmap.append((gtype, size, addr))
    lmap = []
    for _ in range(n_lists):
        addr, num, ltype, _z = struct.unpack_from("<IIII", data, pos)
        pos += 16
        lmap.append((addr, num, ltype))
    lists = []
    for addr, num, ltype in lmap:
        base = addr + 2 * num      # skip entryAddr[num] shorts
        idx = [struct.unpack_from("<hh", data, base + 4 * i)[1] for i in range(num)]
        lists.append((ltype, idx))
    clips = {}
    for gi, (gtype, size, addr) in enumerate(gmap):
        num = (size - 1) // 11
        p = addr + 1
        for _ in range(num):
            race = data[p]
            index = struct.unpack_from("<h", data, p + 8)[0]
            p += 11
            if gtype == 0:
                clips[index] = {"race": race, "group": gi, "groupType": 0, "list": -1, "listType": -1}
            else:
                li = index - V_OFFSET
                if 0 <= li < len(lists):
                    for s in lists[li][1]:
                        clips[s] = {"race": race, "group": gi, "groupType": 1, "list": li, "listType": lists[li][0]}
    return clips


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--res", required=True)
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    with zipfile.ZipFile(a.res) as z:
        name = next(n for n in z.namelist() if n.lower() == "converse.bin")
        data = z.read(name)
    clips = parse(data)
    with open(a.out, "w", encoding="utf-8") as f:
        json.dump({"clips": {str(k): v for k, v in sorted(clips.items())}}, f)
    print("%d clips in %d groups" % (len(clips), len({c["group"] for c in clips.values()})))


if __name__ == "__main__":
    main()
