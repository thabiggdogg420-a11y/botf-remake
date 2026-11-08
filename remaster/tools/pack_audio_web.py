#!/usr/bin/env python3
"""Audio pack step for the browser target: builds the res:// audio the game loads at runtime.

  python pack_audio_web.py --audio <LocalAssets/audio> --out <remaster/godot/scripts/audio/pack> [--ffmpeg ffmpeg] [--jobs 6]

Reads LocalAssets/audio/cues.json (the BOTF banks, remaster/tools/build_audio_cues.py) and LocalAssets/audio/trek/cues.json
(Simon's Trek library, mine_trek_audio.py cues). Writes, under --out (gitignored, private):

  <pack>/<kind>/<name>.ogg      one folder per pack: "shared" plus one per empire (fed kli rom car fer)
  <pack>/cues.json              {"cues": {key: [relative ogg paths]}}, read by BotfAudioLibrary.load_pack
  pack_manifest.json            packs, file counts, bytes, budgets and any violation

The runtime never reads LocalAssets or an env-var path (M0-LANES "Browser target"); this step is the only thing that does.
Formats are the art bible's web budgets (ART-BIBLE 12.1), Ogg Vorbis only, never MP3:
  voice   q2, 32 kHz mono, -18 LUFS (two-pass loudnorm), 120 KB per clip
  sfx     q3, 32 kHz mono, 80 KB per clip (12 essential cues are listed in pack_manifest "preload")
  music   q3, 44.1 kHz stereo, 2.5 MB per track
Pack sizes: the bible fixes 60 MB for the first download and 100 MB per empire pack for everything (hulls, voice, music).
The audio shares assumed here are 15 MB for "shared" and 40 MB per empire; they are the lane's split, not the bible's.
A clip over its budget is re-encoded one quality step lower, then reported if still over (it is kept, flagged).
"""
import argparse
import concurrent.futures as cf
import json
import os
import re
import subprocess
import sys

FACS = ["fed", "kli", "rom", "car", "fer"]
BUDGET = {"voice": 120 * 1024, "sfx": 80 * 1024, "music": int(2.5 * 1024 * 1024)}
PACK_BUDGET = {"shared": 15 * 1024 * 1024}
EMPIRE_BUDGET = 40 * 1024 * 1024
PRELOAD = ["sfx.ui.click", "sfx.ui.beep", "sfx.ui.open", "sfx.ui.close", "sfx.ui.endturn", "sfx.alert.red", "sfx.shield.hit",
           "sfx.explosion.small", "sfx.explosion.large", "weapon.generic.beam", "weapon.generic.torpedo", "sfx.warp.in"]


def kind_of(key):
    if key.startswith("mus."):
        return "music"
    if key.startswith("vo."):
        return "voice"
    return "sfx"


def pack_of(key):
    """The empire a key belongs to (a faction token in the key), else 'shared'. Music for a faction goes with it."""
    parts = key.split(".")
    for p in reversed(parts):
        if p in FACS:
            return p
    return "shared"


def run(cmd):
    return subprocess.run(cmd, capture_output=True, text=True, encoding="utf-8", errors="replace")


def loudnorm_args(ffmpeg, src):
    """Two-pass -18 LUFS: measure, then a linear gain. Falls back to single pass when the measure fails."""
    r = run([ffmpeg, "-v", "info", "-i", src, "-af", "loudnorm=I=-18:TP=-1.5:LRA=11:print_format=json", "-f", "null", "-"])
    m = re.search(r"\{[^{}]*\"input_i\"[^{}]*\}", r.stderr, re.S)
    try:
        j = json.loads(m.group(0))
        if float(j["input_i"]) < -60:
            return "loudnorm=I=-18:TP=-1.5:LRA=11"
        return ("loudnorm=I=-18:TP=-1.5:LRA=11:measured_I=%s:measured_TP=%s:measured_LRA=%s:measured_thresh=%s:offset=%s:linear=true"
                % (j["input_i"], j["input_tp"], j["input_lra"], j["input_thresh"], j["target_offset"]))
    except Exception:
        return "loudnorm=I=-18:TP=-1.5:LRA=11"


def encode(ffmpeg, src, dst, kind, q):
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    if kind == "music":
        args = ["-ar", "44100", "-ac", "2", "-c:a", "libvorbis", "-q:a", str(q)]
        af = []
    else:
        args = ["-ar", "32000", "-ac", "1", "-c:a", "libvorbis", "-q:a", str(q)]
        af = ["-af", loudnorm_args(ffmpeg, src) + ",aresample=32000"] if kind == "voice" else []
    r = run([ffmpeg, "-v", "error", "-y", "-i", src] + af + args + [dst])
    return r.returncode == 0 and os.path.exists(dst)


def job(a):
    ffmpeg, src, dst, kind = a
    q = {"voice": 2, "sfx": 3, "music": 3}[kind]
    if os.path.exists(dst) and os.path.getmtime(dst) >= os.path.getmtime(src):
        pass
    elif not encode(ffmpeg, src, dst, kind, q):
        return dst, 0, "encode failed"
    note = ""
    size = os.path.getsize(dst)
    while size > BUDGET[kind] and q > -1:
        q -= 1 if kind != "music" else 1
        if not encode(ffmpeg, src, dst, kind, q):
            break
        size = os.path.getsize(dst)
        note = "re-encoded at q%d" % q
    if size > BUDGET[kind]:
        note = "OVER BUDGET %d > %d" % (size, BUDGET[kind])
    return dst, size, note


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--audio", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--ffmpeg", default=os.environ.get("FFMPEG", "ffmpeg"))
    ap.add_argument("--jobs", type=int, default=6)
    ap.add_argument("--only", help="comma list of key prefixes to pack (default all)")
    a = ap.parse_args()

    tables = []
    for rel in ["cues.json", "trek/cues.json"]:
        p = os.path.join(a.audio, rel)
        if os.path.exists(p):
            tables.append((os.path.dirname(p), json.load(open(p, encoding="utf-8"))["cues"]))
    if not tables:
        print("no cues.json under", a.audio, file=sys.stderr)
        return 2
    prefixes = a.only.split(",") if a.only else None

    jobs, plan = {}, {}   # dst -> job, (pack, key) -> [dst rel]
    for base, cues in tables:
        for key, files in cues.items():
            if prefixes and not any(key.startswith(p) for p in prefixes):
                continue
            kind, pack = kind_of(key), pack_of(key)
            for f in files:
                src = os.path.normpath(os.path.join(base, f))
                if not os.path.exists(src):
                    continue
                name = os.path.splitext(f.replace("\\", "/"))[0].replace("/", "_") + ".ogg"
                rel = "%s/%s" % (kind, name)
                dst = os.path.join(a.out, pack, rel)
                jobs[dst] = (a.ffmpeg, src, dst, kind)
                plan.setdefault((pack, key), [])
                if rel not in plan[(pack, key)]:
                    plan[(pack, key)].append(rel)

    print("%d files to pack" % len(jobs), file=sys.stderr)
    sizes, notes = {}, []
    with cf.ThreadPoolExecutor(a.jobs) as ex:
        for i, (dst, size, note) in enumerate(ex.map(job, jobs.values())):
            sizes[dst] = size
            if note:
                notes.append((dst, note))
            if i % 200 == 0:
                print(i, "done", file=sys.stderr)

    packs = {}
    for (pack, key), rels in sorted(plan.items()):
        ok = [r for r in rels if sizes.get(os.path.join(a.out, pack, r), 0) > 0]
        if ok:
            packs.setdefault(pack, {})[key] = ok
    manifest = {"packs": {}, "budgets": {"clip_bytes": BUDGET, "shared_pack_bytes": PACK_BUDGET["shared"], "empire_pack_bytes": EMPIRE_BUDGET},
                "preload": PRELOAD, "violations": []}
    for pack, cues in packs.items():
        os.makedirs(os.path.join(a.out, pack), exist_ok=True)
        json.dump({"cues": cues, "note": "private, generated by tools/pack_audio_web.py; ogg only"},
                  open(os.path.join(a.out, pack, "cues.json"), "w"), indent=0)
        total = sum(sizes.get(os.path.join(a.out, pack, r), 0) for rs in cues.values() for r in set(rs))
        lim = PACK_BUDGET.get(pack, EMPIRE_BUDGET)
        manifest["packs"][pack] = {"keys": len(cues), "files": len({r for rs in cues.values() for r in rs}), "bytes": total, "budget": lim}
        if total > lim:
            manifest["violations"].append("pack %s is %.1f MB over its %.0f MB share" % (pack, total / 1048576, lim / 1048576))
    for dst, note in notes:
        if note.startswith("OVER") or note == "encode failed":
            manifest["violations"].append("%s: %s" % (os.path.relpath(dst, a.out).replace("\\", "/"), note))
    json.dump(manifest, open(os.path.join(a.out, "pack_manifest.json"), "w"), indent=1)
    for k, v in manifest["packs"].items():
        print("%-7s %4d keys %5d files %7.1f MB (budget %.0f MB)" % (k, v["keys"], v["files"], v["bytes"] / 1048576, v["budget"] / 1048576))
    print("violations:", len(manifest["violations"]))
    for v in manifest["violations"][:20]:
        print("  ", v)
    return 0


if __name__ == "__main__":
    sys.exit(main())
