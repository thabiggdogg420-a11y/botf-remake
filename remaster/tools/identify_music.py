#!/usr/bin/env python3
"""F5 music identification: measures every segment of the original BOTF soundtrack (music.snd, extracted to
game/LocalAssets/audio/music/music_tNN_sMM.ogg by convert_botf_audio.py) and writes what each track and segment is.

Per segment: length, loudness (RMS dBFS), brightness (spectral centroid), pulse (onset rate), key profile (chroma).
Per track: which race and role it is (title, strategic, combat, or the unlabelled tracks 11-15), judged by measurement:
  - tracks 11-15 are compared with every other track by chroma and loudness, so each is named by what it sounds closest to;
  - segment joins are tested (the last 50 ms of one segment against the first 50 ms of the next) to show which segments run
    on seamlessly, which is what the adaptive player needs to move between them on a segment boundary.
Every segment gets an intensity tier inside its own track (calm / build / peak by loudness and pulse), used by the adaptive
music (godot/scripts/audio/botf_music_director.gd reads the JSON).

    python tools/identify_music.py --music game/LocalAssets/audio/music --json godot/scripts/audio/music_identified.json --md docs/MUSIC-IDENTIFIED.md

Needs ffmpeg on PATH and numpy. Private inputs stay local; the JSON holds only measurements and file names.
"""
import argparse
import json
import os
import re
import subprocess

import numpy as np

SR = 22050
RACES = {0: "Cardassian", 1: "Federation", 2: "Ferengi", 3: "Klingon", 4: "Romulan"}
FAC = {0: "car", 1: "fed", 2: "fer", 3: "kli", 4: "rom"}


def decode(path):
    raw = subprocess.run(["ffmpeg", "-v", "quiet", "-i", path, "-ac", "1", "-ar", str(SR), "-f", "s16le", "-"],
                         check=True, capture_output=True).stdout
    return np.frombuffer(raw, dtype=np.int16).astype(np.float32) / 32768.0


def frames(x, n=2048, hop=1024):
    if len(x) < n:
        x = np.pad(x, (0, n - len(x)))
    count = 1 + (len(x) - n) // hop
    idx = np.arange(n)[None, :] + hop * np.arange(count)[:, None]
    return x[idx] * np.hanning(n)[None, :]


def features(x):
    f = frames(x)
    mag = np.abs(np.fft.rfft(f, axis=1))
    freqs = np.fft.rfftfreq(2048, 1.0 / SR)
    energy = mag.sum(axis=1) + 1e-9
    centroid = float(np.median((mag * freqs[None, :]).sum(axis=1) / energy))
    # onset rate: positive spectral flux peaks per second
    flux = np.maximum(0.0, np.diff(mag, axis=0)).sum(axis=1)
    thr = np.median(flux) + 1.5 * np.std(flux)
    peaks = np.sum((flux[1:-1] > thr) & (flux[1:-1] >= flux[:-2]) & (flux[1:-1] >= flux[2:]))
    seconds = len(x) / SR
    # chroma (12 pitch classes, 55 Hz - 5 kHz)
    chroma = np.zeros(12)
    sel = (freqs > 55) & (freqs < 5000)
    pcs = (np.round(12 * np.log2(freqs[sel] / 440.0)) % 12).astype(int)
    power = (mag[:, sel] ** 2).sum(axis=0)
    np.add.at(chroma, pcs, power)
    chroma = chroma / (np.linalg.norm(chroma) + 1e-9)
    rms = float(np.sqrt(np.mean(x ** 2)) + 1e-9)
    return {
        "seconds": round(seconds, 2),
        "rms_db": round(20 * np.log10(rms), 1),
        "centroid_hz": round(centroid),
        "onsets_per_s": round(float(peaks) / max(seconds, 0.1), 2),
        "chroma": [round(float(c), 4) for c in chroma],
        "head": x[: int(0.05 * SR)].tolist(),
        "tail": x[-int(0.05 * SR):].tolist(),
    }


def seam(a_tail, b_head):
    """Level jump at the join (dB). Small means the segments run on without a gap or step."""
    a = np.sqrt(np.mean(np.square(a_tail)) + 1e-9)
    b = np.sqrt(np.mean(np.square(b_head)) + 1e-9)
    return round(float(abs(20 * np.log10(a / b))), 1)


def role(track):
    if track == 0:
        return "title", None
    if 1 <= track <= 5:
        return "strategic", track - 1
    if 6 <= track <= 10:
        return "combat", track - 6
    return "unlabelled", track - 11


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--music", required=True)
    ap.add_argument("--json", required=True)
    ap.add_argument("--md", required=True)
    a = ap.parse_args()
    tracks = {}
    for f in sorted(os.listdir(a.music)):
        m = re.match(r"music_t(\d+)_s(\d+)\.ogg$", f)
        if m:
            tracks.setdefault(int(m[1]), []).append((int(m[2]), f))
    seg = {}
    for t, items in tracks.items():
        for s, f in sorted(items):
            seg[(t, s)] = features(decode(os.path.join(a.music, f)))
            seg[(t, s)]["file"] = f
    # Track profiles
    prof = {}
    for t, items in tracks.items():
        ss = [seg[(t, s)] for s, _ in sorted(items)]
        w = np.array([x["seconds"] for x in ss])
        chroma = np.average(np.array([x["chroma"] for x in ss]), axis=0, weights=w)
        prof[t] = {
            "chroma": chroma / (np.linalg.norm(chroma) + 1e-9),
            "rms_db": float(np.average([x["rms_db"] for x in ss], weights=w)),
            "onsets": float(np.average([x["onsets_per_s"] for x in ss], weights=w)),
            "centroid": float(np.average([x["centroid_hz"] for x in ss], weights=w)),
            "seconds": float(w.sum()),
        }
    out = {"note": "F5 music identification (tools/identify_music.py). Measurements only.", "tracks": []}
    lines = ["# Music identified (F5)", "",
             "Outranked by: Simon's words in `AI_stuff\\standing\\simon\\`; docs/AUDIO-PLAN.md for the extraction. Made by "
             "`tools/identify_music.py` from the original soundtrack (music.snd, 16 tracks). Measured, not heard: Simon's ear "
             "overrules any line here.", "",
             "Race order in music.snd is the game's race index: 0 Cardassian, 1 Federation, 2 Ferengi, 3 Klingon, 4 Romulan "
             "(the same order converse.bin uses for the voice).", "",
             "Columns: length, loudness (RMS dBFS), brightness (spectral centroid), pulse (onsets per second), intensity tier "
             "inside the track, and the level step at the join into the next segment (under 6 dB runs on).", ""]
    for t in sorted(tracks):
        r, race = role(t)
        items = sorted(tracks[t])
        # intensity tiers inside the track: score = loudness z + pulse z
        rms = np.array([seg[(t, s)]["rms_db"] for s, _ in items])
        ons = np.array([seg[(t, s)]["onsets_per_s"] for s, _ in items])
        z = (rms - rms.mean()) / (rms.std() + 1e-9) + (ons - ons.mean()) / (ons.std() + 1e-9)
        order = np.argsort(z, kind="stable")
        tier = {}
        n = len(items)
        for rank, i in enumerate(order):
            tier[items[i][0]] = "calm" if rank < n / 3 else "build" if rank < 2 * n / 3 else "peak"
        entry = {"track": t, "role": r, "race": RACES.get(race) if race is not None else None,
                 "fac": FAC.get(race) if race is not None else None, "segments": []}
        nearest = None
        if r == "unlabelled":
            scores = []
            for u in sorted(tracks):
                if u == t or u >= 11:
                    continue
                c = float(np.dot(prof[t]["chroma"], prof[u]["chroma"]))
                scores.append((c, u))
            scores.sort(reverse=True)
            nearest = scores[:3]
            best = nearest[0][1]
            br, brace = role(best)
            # Tracks 11-15 sit in race order after the combat block and run about a minute each, 3-4 segments, quieter than
            # combat. The key profile does not tie any of them to another track (every match is 0.8-0.95, no clear winner),
            # so they are named by placement and length only: a short race cue. Which screen used them is unconfirmed.
            strategic = prof.get(1 + race)
            louder = strategic is not None and prof[t]["rms_db"] > strategic["rms_db"]
            entry["identified_as"] = f"short race cue ({RACES[race]}; {'louder' if louder else 'quieter'} than its strategic theme; screen unconfirmed)"
            entry["nearest"] = [{"track": u, "chroma_match": round(c, 3), "role": role(u)[0],
                                 "race": RACES.get(role(u)[1]) if role(u)[1] is not None else None} for c, u in nearest]
        else:
            entry["identified_as"] = {"title": "title theme", "strategic": f"strategic map theme ({RACES.get(race)})",
                                      "combat": f"combat theme ({RACES.get(race)})"}[r]
        hdr = f"## Track {t:02d}: {entry['identified_as']}"
        lines += [hdr, "",
                  f"{len(items)} segments, {prof[t]['seconds'] / 60:.1f} min, loudness {prof[t]['rms_db']:.1f} dB, pulse "
                  f"{prof[t]['onsets']:.2f}/s, brightness {prof[t]['centroid']:.0f} Hz."]
        if nearest:
            lines.append("Closest by key profile: " + ", ".join(
                f"track {u:02d} ({role(u)[0]}{', ' + RACES[role(u)[1]] if role(u)[1] is not None else ''}) {c:.3f}" for c, u in nearest) + ".")
        lines += ["", "| Segment | Length | Loudness | Brightness | Pulse | Tier | Join to next |", "|---|---|---|---|---|---|---|"]
        for k, (s, f) in enumerate(items):
            x = seg[(t, s)]
            join = seam(x["tail"], seg[(t, items[k + 1][0])]["head"]) if k + 1 < len(items) else None
            entry["segments"].append({"segment": s, "file": f, "seconds": x["seconds"], "rms_db": x["rms_db"],
                                      "centroid_hz": x["centroid_hz"], "onsets_per_s": x["onsets_per_s"], "tier": tier[s],
                                      "join_db": join})
            lines.append(f"| s{s:02d} | {x['seconds']:.1f} s | {x['rms_db']:.1f} dB | {x['centroid_hz']} Hz | {x['onsets_per_s']:.2f}/s | "
                         f"{tier[s]} | {'-' if join is None else str(join) + ' dB'} |")
        lines.append("")
        out["tracks"].append(entry)
    os.makedirs(os.path.dirname(os.path.abspath(a.json)), exist_ok=True)
    json.dump(out, open(a.json, "w", encoding="utf-8"), indent=1)
    open(a.md, "w", encoding="utf-8").write("\n".join(lines) + "\n")
    print("wrote", a.json, a.md)


if __name__ == "__main__":
    main()
