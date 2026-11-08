#!/usr/bin/env python3
"""Mines Simon's own Trek library (private, read only, never committed) for the BOTF remaster's audio.

Offline pre-build step: the game runtime never reads these disks (M0-LANES "Browser target"). Outputs go to
LocalAssets/audio/trek/ (gitignored). Every pick is a CANDIDATE that Simon confirms by ear; `sheet` writes the listen
sheet remaster/docs/AUDIO-LISTEN-SHEET.md from the manifest.

  python mine_trek_audio.py index  --root "D:/TV Shows" --out index.json
  python mine_trek_audio.py scan   --index index.json --era tng --episodes "Yesterday" "Best of Both" --out cand.json
  python mine_trek_audio.py cut    --cand cand.json --out <LocalAssets/audio/trek> [--per-class 2]
  python mine_trek_audio.py cue    --index index.json --era ds9 --out <trek dir>   (sound after a subtitle line: red alert,
                                    transporter, warp, cloak) and dialogue lines (ack, engage, fire, torpedo away...)
  python mine_trek_audio.py alert  --index index.json --era tng --episodes "Q Who" --out <trek dir>   (klaxon by spectrum)
  python mine_trek_audio.py cues   --trek <trek dir>      writes <trek dir>/cues.json (era keys for the game)
  python mine_trek_audio.py sheet  --trek <trek dir> --out AUDIO-LISTEN-SHEET.md

Method, sound effects. The 5.1 mix is folded to a mono "no centre" signal (front L/R and surrounds; dialogue sits in the
centre channel); a stereo mix uses the side signal (L-R) for scanning. Moments are onsets (a rise of 10 dB or more over
the half second before, 75th percentile), described by level, spectral centroid, flatness, decay, isolation (rise over the pre-roll) and
dialogue (centre channel level against the fold at the moment). Class names come from the spectrum and are guesses.
Clips are cut from the fold (5.1) or the full mix (stereo), peak-normalised to -3 dBFS, 32 kHz mono wav.
Method, dialogue. A subtitle track in text form (subrip) gives the lines and times. A phrase table picks the lines;
they are cut from the centre channel (5.1) or the mid (stereo); "dry" is how far the centre is above the fold.
Episodes whose subtitles are bitmaps (PGS/DVD) are searched by spectrum only (klaxon) and give no dialogue.
"""
import argparse
import json
import os
import re
import subprocess
import sys

import numpy as np

FFMPEG = os.environ.get("FFMPEG", "ffmpeg")
FFPROBE = os.environ.get("FFPROBE", "ffprobe")
SR = 22050
OUT_SR = 32000
ERAS = {"tos": "Star Trek.TOS", "tng": "Star Trek TNG", "ds9": "Star Trek Deep Space 9",
        "voy": "Star Trek - Voyager", "ent": "Star Trek Enterprise Season"}
EP_RE = re.compile(r"S(\d{1,2})E(\d{2})|Season (\d\d) Episode (\d\d)", re.I)
VIDEO_EXT = (".mkv", ".mp4", ".avi")
TEXT_SUB = ("subrip", "ass", "mov_text", "srt")


def run(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, **kw)


def probe(path):
    r = run([FFPROBE, "-v", "error", "-show_entries",
             "stream=index,codec_type,codec_name,channels,channel_layout:stream_tags=language",
             "-show_entries", "format=duration", "-of", "json", path], text=True, encoding="utf-8", errors="replace")
    try:
        return json.loads(r.stdout)
    except Exception:
        return {}


def cmd_index(a):
    out = []
    for era, folder in ERAS.items():
        base = os.path.join(a.root, [d for d in os.listdir(a.root) if d.startswith(folder)][0])
        for dp, dn, fn in os.walk(base):
            if any(x in dp for x in ("Extras", "Featurettes", "Special")):
                continue
            for f in sorted(fn):
                if not f.lower().endswith(VIDEO_EXT):
                    continue
                p = os.path.join(dp, f)
                info = probe(p)
                aud = [s for s in info.get("streams", []) if s.get("codec_type") == "audio"]
                sub = [s for s in info.get("streams", []) if s.get("codec_type") == "subtitle"]
                m = EP_RE.search(f)
                out.append({"era": era, "path": p.replace("\\", "/"), "name": f,
                            "season": int(m.group(1) or m.group(3)) if m else 0,
                            "episode": int(m.group(2) or m.group(4)) if m else 0,
                            "duration": float(info.get("format", {}).get("duration", 0) or 0),
                            "audio": [{"index": s["index"], "ch": s.get("channels", 0), "layout": s.get("channel_layout", ""),
                                       "codec": s.get("codec_name", "")} for s in aud],
                            "text_subs": [s["index"] for s in sub if s.get("codec_name") in TEXT_SUB
                                          and s.get("tags", {}).get("language", "eng") == "eng"],
                            "bitmap_subs": sum(1 for s in sub if s.get("codec_name") not in TEXT_SUB)})
        print(era, sum(1 for e in out if e["era"] == era), "episodes", file=sys.stderr)
    json.dump(out, open(a.out, "w"), indent=1)


# ---------------------------------------------------------------------------------------------------------
# decoding
# ---------------------------------------------------------------------------------------------------------

def fold_filters(ch):
    """(fold, centre) pan filters. fold = what dialogue is not in; centre = where dialogue is."""
    if ch >= 6:   # FL FR FC LFE BL BR (5.1(side) keeps the same channel order)
        return "pan=mono|c0=0.5*c0+0.5*c1+0.35*c4+0.35*c5", "pan=mono|c0=c2"
    return "pan=mono|c0=0.5*c0-0.5*c1", "pan=mono|c0=0.5*c0+0.5*c1"


def cut_filters(ch):
    """(effects cut, voice cut): the fold for 5.1 effects, the whole mix for stereo effects; the centre for voice."""
    if ch >= 6:
        return "pan=mono|c0=0.5*c0+0.5*c1+0.35*c4+0.35*c5+0.4*c3", "pan=mono|c0=c2"
    return "pan=mono|c0=0.5*c0+0.5*c1", "pan=mono|c0=0.5*c0+0.5*c1"


def decode_one(path, apos, flt, start=None, dur=None, sr=SR):
    pre = []
    if start is not None:
        pre += ["-ss", "%.3f" % max(0.0, start)]
    if dur is not None:
        pre += ["-t", "%.3f" % dur]
    r = run([FFMPEG, "-v", "error"] + pre + ["-i", path, "-map", "0:a:%d" % apos, "-af", flt, "-ar", str(sr),
                                                "-f", "f32le", "-"])
    return np.frombuffer(r.stdout, dtype=np.float32)


def decode(path, ch, start=None, dur=None, apos=0):
    nc, ce = (decode_one(path, apos, f, start, dur) for f in fold_filters(ch))
    n = min(len(nc), len(ce))
    return nc[:n], ce[:n]


def db(x, floor=1e-7):
    return 20 * np.log10(np.maximum(x, floor))


def frame_rms(x, hop):
    n = len(x) // hop
    return np.sqrt(np.mean(x[:n * hop].reshape(n, hop) ** 2, axis=1)) if n else np.zeros(0)


def describe(seg):
    """Spectral centroid (Hz), flatness (0-1), share of energy under 200 Hz."""
    if len(seg) < 512:
        return 0, 0, 0
    sp = np.abs(np.fft.rfft(seg * np.hanning(len(seg)))) ** 2 + 1e-12
    fr = np.fft.rfftfreq(len(seg), 1 / SR)
    return (float(np.sum(fr * sp) / np.sum(sp)), float(np.exp(np.mean(np.log(sp))) / np.mean(sp)),
            float(np.sum(sp[fr < 200]) / np.sum(sp)))


def classify(cen, flat, low, decay):
    """Heuristic class from the spectrum. A GUESS: the listen sheet is how Simon confirms it."""
    if decay >= 2.0 and low > 0.5:
        return "explosion_large"
    if decay >= 0.6 and low > 0.35:
        return "explosion_or_impact"
    if cen > 2500 and flat < 0.05 and decay >= 0.4:
        return "beam_or_sweep"
    if cen > 1800 and decay < 0.6:
        return "shield_or_hit"
    if 0.6 <= decay < 2.0 and low <= 0.35 and flat < 0.2:
        return "launch_or_warp"
    return "other"


def scan_episode(ep, rise_db=10.0, centre_margin=-22.0):
    aud = ep["audio"][0]
    nc, ce = decode(ep["path"], aud["ch"])
    hop = int(SR * 0.02)
    dn, dc = db(frame_rms(nc, hop)), db(frame_rms(ce, hop))
    fps = SR / hop
    pre = int(0.5 * fps)
    out, last = [], -10
    floor_db = float(np.percentile(dn, 90))   # a moment must be louder than 90% of the episode's fold
    for i in range(pre, len(dn) - int(3 * fps)):
        if dn[i] < floor_db or dn[i] < dn[i - 1]:
            continue
        before = float(np.percentile(dn[i - pre:i - 1], 75))
        if dn[i] - before < rise_db:
            continue
        j = i + int(np.argmax(dn[i:i + int(0.15 * fps)]))
        t = i / fps
        if t - last < 1.0:
            continue
        tail = dn[j:j + int(8 * fps)]
        below = np.where(tail < dn[j] - 24)[0]
        decay = (below[0] if len(below) else len(tail)) / fps
        cen, flat, low = describe(nc[i * hop:i * hop + int(min(decay + 0.05, 2.0) * SR)])
        dialogue = float(np.max(dc[i:j + int(max(decay, 0.3) * fps)])) - float(dn[j])
        if aud["ch"] >= 6 and dialogue > -centre_margin:   # the centre is far louder than the fold: speech
            continue
        out.append({"t": round(t, 2), "peak_db": round(float(dn[j]), 1), "rise_db": round(float(dn[j] - before), 1),
                    "decay_s": round(decay, 2), "centroid_hz": int(cen), "flatness": round(flat, 3), "low200": round(low, 2),
                    "dialogue_db": round(dialogue, 1), "class": classify(cen, flat, low, decay)})
        last = t
    return out


def pick(idx, era, names):
    return [e for e in idx if e["era"] == era and any(k.lower() in e["name"].lower() for k in names)]


def cmd_scan(a):
    idx = json.load(open(a.index))
    res = []
    for ep in pick(idx, a.era, a.episodes):
        c = scan_episode(ep)
        c.sort(key=lambda m: -m["rise_db"] - 0.2 * m["peak_db"])
        res.append({"era": ep["era"], "path": ep["path"], "name": ep["name"], "audio": ep["audio"][0], "moments": c[:a.top]})
        print(ep["name"], len(c), "moments", file=sys.stderr)
    json.dump(res, open(a.out, "w"), indent=1)


# ---------------------------------------------------------------------------------------------------------
# cutting
# ---------------------------------------------------------------------------------------------------------

def slug(s):
    return re.sub(r"[^a-z0-9]+", "-", s.lower()).strip("-")[:40]


def ep_tag(name):
    m = EP_RE.search(name)
    if m:
        return "s%02de%02d" % (int(m.group(1) or m.group(3)), int(m.group(2) or m.group(4)))
    return slug(name)[:20]


def write_clip(path, apos, flt, start, dur, out_wav, fade_out=0.04):
    """Cuts, trims silence at the tail, peak-normalises to -3 dBFS and writes 32 kHz mono 16-bit wav. Returns the
    clip's real duration and its pre-normalisation peak (dBFS), or None when the cut is empty."""
    x = decode_one(path, apos, flt, start, dur, OUT_SR)
    if len(x) < OUT_SR * 0.08:
        return None
    pk = float(np.max(np.abs(x)))
    if pk < 1e-4:
        return None
    env = np.abs(x)
    k = int(0.02 * OUT_SR)
    sm = np.convolve(env, np.ones(k) / k, mode="same")
    live = np.where(sm > pk * 0.01)[0]
    end = min(len(x), int(live[-1] + 0.05 * OUT_SR)) if len(live) else len(x)
    x = x[:end] * (10 ** (-3 / 20) / pk)
    f = min(int(fade_out * OUT_SR), len(x) // 2)
    x[-f:] *= np.linspace(1, 0, f)
    g = min(int(0.004 * OUT_SR), len(x) // 2)
    x[:g] *= np.linspace(0, 1, g)
    pcm = (np.clip(x, -1, 1) * 32767).astype("<i2").tobytes()
    os.makedirs(os.path.dirname(out_wav), exist_ok=True)
    import wave
    with wave.open(out_wav, "wb") as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(OUT_SR); w.writeframes(pcm)
    return len(x) / OUT_SR, float(db(pk))


KIND_FOR_CLASS = {"beam_or_sweep": "beam", "explosion_or_impact": "impact", "explosion_large": "explosion",
                  "shield_or_hit": "hit", "launch_or_warp": "launch"}


def manifest_add(trek, rec):
    mp = os.path.join(trek, "manifest.%s.json" % rec["era"])   # one file per era: eras run in parallel
    m = json.load(open(mp)) if os.path.exists(mp) else {"clips": []}
    m["clips"] = [c for c in m["clips"] if c["file"] != rec["file"]] + [rec]
    json.dump(m, open(mp, "w"), indent=1)


def cmd_cut(a):
    for ep in json.load(open(a.cand)):
        aud = ep["audio"]
        flt, _ = cut_filters(aud["ch"])
        by = {}
        for m in ep["moments"]:
            by.setdefault(m["class"], []).append(m)
        for cls, ms in by.items():
            kind = KIND_FOR_CLASS.get(cls)
            if not kind:
                continue
            ms.sort(key=lambda m: -(m["rise_db"] - 0.3 * abs(m["dialogue_db"] if aud["ch"] >= 6 else 0) + 0.2 * m["peak_db"]))
            for m in ms[:a.per_class]:
                dur = min(max(m["decay_s"] + 0.2, 0.5), 4.0)
                rel = "%s/%s/%s_%s_%d.wav" % (ep["era"], kind, kind, ep_tag(ep["name"]), int(m["t"]))
                got = write_clip(ep["path"], 0, flt, m["t"] - 0.06, dur,
                                 os.path.join(a.out, rel))
                if got:
                    manifest_add(a.out, {"era": ep["era"], "kind": kind, "guess": cls, "file": rel, "source": ep["name"],
                                         "source_path": ep["path"], "t": m["t"], "dur": round(got[0], 2),
                                         "metrics": {k: m[k] for k in ("peak_db", "rise_db", "centroid_hz", "flatness", "low200", "dialogue_db", "decay_s")},
                                         "status": "candidate"})


# ---------------------------------------------------------------------------------------------------------
# subtitles: dialogue lines and the sound that follows a line
# ---------------------------------------------------------------------------------------------------------

def read_srt(path, stream_index):
    r = run([FFMPEG, "-v", "error", "-i", path, "-map", "0:%d" % stream_index, "-f", "srt", "-"])
    txt = r.stdout.decode("utf-8", errors="replace")
    out = []
    for blk in re.split(r"\r?\n\r?\n", txt):
        m = re.search(r"(\d+):(\d+):(\d+)[,.](\d+) --> (\d+):(\d+):(\d+)[,.](\d+)\s*\r?\n(.*)", blk, re.S)
        if not m:
            continue
        g = [int(x) for x in m.groups()[:8]]
        text = re.sub(r"<[^>]+>|\{[^}]+\}", "", m.group(9))
        text = re.sub(r"\s+", " ", text).strip()
        out.append((g[0] * 3600 + g[1] * 60 + g[2] + g[3] / 1000, g[4] * 3600 + g[5] * 60 + g[6] + g[7] / 1000, text))
    return out


# key -> (regex on the whole subtitle line, max words)
FED_LINES = {
    "ack": (r"^(aye,? (sir|captain|ma'am)|yes,? (sir|captain)|understood|acknowledged|aye aye)[.,!]?$", 3),
    "engage": (r"^engage[.!]?$", 1),
    "fire": (r"^fire[.!]*$|^fire (phasers|at will|all weapons|torpedoes)[.!]?$", 3),
    "torpedo_away": (r"^(torpedoes? (away|launched|fired|away)|firing (photon )?torpedoes?|photon torpedoes? (away|fired))[.!]?$", 4),
    "red_alert": (r"^(red alert|go to red alert|set condition red)[.!]*$", 4),
    "shields_up": (r"^(raise (the )?shields|shields up|raising shields)[.!]*$", 3),
    "battle_stations": (r"^(all hands,? )?(to )?battle stations[.!]*$", 4),
    "evasive": (r"^evasive (action|manoeuvres|maneuvers)[.!]*$|^evasive pattern \w+[.!]*$", 3),
    "target_locked": (r"^(target(s)? (locked|acquired)|locked on|weapons locked)[.!]*$", 3),
    "shields_down": (r"^shields (are )?(down|failing|buckling|at \d+ percent)[.!]*$", 5),
    "hull_breach": (r"^hull breach[.!]*$|^direct hit[.!]*$", 2),
    "all_stop": (r"^(all stop|hold your fire|cease fire)[.!]*$", 2),
    "hail": (r"^(on screen|open a channel|hailing frequencies open)[.!]*$", 3),
    "energize": (r"^energi[sz]e[.!]*$", 1),
    "make_it_so": (r"^make it so[.!]*$", 3),
}
KLI_LINES = {
    "ack": (r"^(yes,? (captain|sir|commander)|it (will be|is) done|as you wish)[.!]*$", 4),
    "fire": (r"^fire[.!]*$", 1),
    "good_day": (r"^today is a good day to die[.!]*$", 6),
    "qapla": (r"^qapla'?[.!]*$", 1),
    "cloak": (r"^(cloak|engage the cloak|decloak|cloaking device)[.!]*$", 4),
}
# the sound that follows a spoken line: key -> (line key, seconds to listen, minimum peak dBFS of the fold)
AFTER_LINE = {"alert.red": ("red_alert", 6.0), "transporter": ("energize", 5.0), "warp": ("engage", 4.0)}


def cmd_cue(a):
    idx = json.load(open(a.index))
    eps = [e for e in idx if e["era"] == a.era and e["text_subs"]]
    if a.episodes:
        eps = [e for e in eps if any(k.lower() in e["name"].lower() for k in a.episodes)]
    eps = eps[:a.max_episodes]
    cands = {}
    for ep in eps:
        aud = ep["audio"][0]
        subs = read_srt(ep["path"], ep["text_subs"][0])
        print(ep["name"], len(subs), "subtitle lines", file=sys.stderr)
        for who, table in (("fed", FED_LINES), ("kli", KLI_LINES)):
            for key, (rx, maxw) in table.items():
                for n, (s, e, text) in enumerate(subs):
                    t = text.replace("-", " ").strip()
                    if len(t.split()) > maxw or not re.search(rx, t.lower().strip(), re.I) or e - s > 4.0:
                        continue
                    nxt = subs[n + 1][0] if n + 1 < len(subs) else 1e9
                    prv = subs[n - 1][1] if n else -1e9
                    cands.setdefault((who, key), []).append({"ep": ep, "s": s, "e": e, "text": text, "next": nxt, "prev": prv})
    print({"%s.%s" % k: len(v) for k, v in cands.items()}, file=sys.stderr)
    # score each candidate by how dry the voice is (centre over fold), cut the best few per key
    vflt = {}
    for (who, key), lst in sorted(cands.items()):
        scored = []
        for c in lst[::max(1, len(lst) // 24)]:   # spread over the episodes; each one costs two decodes
            aud = c["ep"]["audio"][0]
            if aud["ch"] < 6:
                dry = 0.0
            else:
                nc, ce = decode(c["ep"]["path"], aud["ch"], c["s"], max(c["e"] - c["s"], 0.3))
                if len(nc) < 800:
                    continue
                dry = float(db(np.sqrt(np.mean(ce ** 2))) - db(np.sqrt(np.mean(nc ** 2))))
            gap = min(c["s"] - c["prev"], c["next"] - c["e"])
            scored.append((dry + min(gap, 1.5) * 4 - (e_len(c)) * 2, dry, c))
        scored.sort(key=lambda x: -x[0])
        taken, seen_ep = 0, set()
        for sc, dry, c in scored:
            tag = ep_tag(c["ep"]["name"])
            if a.per_key and taken >= a.per_key:
                break
            if (tag, int(c["s"])) in seen_ep:
                continue
            seen_ep.add((tag, int(c["s"])))
            aud = c["ep"]["audio"][0]
            _, voice_f = cut_filters(aud["ch"])
            start = max(c["s"] - 0.08, c["prev"] + 0.02)
            end = min(c["e"] + 0.18, c["next"] - 0.02)
            rel = "%s/voice/%s_%s_%s_%d.wav" % (a.era, who, key, tag, int(c["s"]))
            got = write_clip(c["ep"]["path"], 0, voice_f, start, end - start,
                             os.path.join(a.out, rel), fade_out=0.06)
            if got:
                taken += 1
                manifest_add(a.out, {"era": a.era, "kind": "voice", "voice_key": "%s.%s" % (who, key), "file": rel,
                                     "source": c["ep"]["name"], "source_path": c["ep"]["path"], "t": round(start, 2),
                                     "dur": round(got[0], 2), "text": c["text"], "dry_db": round(dry, 1),
                                     "stereo_source": aud["ch"] < 6, "status": "candidate"})
    # sound after a line
    for kind, (linekey, listen) in AFTER_LINE.items():
        lst = cands.get(("fed", linekey), [])
        lst = lst[::max(1, len(lst) // 16)]
        res = []
        for c in lst:
            aud = c["ep"]["audio"][0]
            win = min(listen, max(0.5, c["next"] - c["e"]))
            if win < 1.5:
                continue
            nc, ce = decode(c["ep"]["path"], aud["ch"], c["e"], win)
            if len(nc) < SR:
                continue
            rn = db(frame_rms(nc, int(SR * 0.05)))
            rc = db(frame_rms(ce, int(SR * 0.05)))
            level = float(np.percentile(rn, 80))
            dlg = float(np.percentile(rc, 80) - level) if aud["ch"] >= 6 else 0.0
            res.append((level - max(dlg + 6, 0) * 0.5 + win, level, dlg, win, c))
        res.sort(key=lambda x: -x[0])
        for sc, level, dlg, win, c in res[:a.per_key]:
            aud = c["ep"]["audio"][0]
            flt, _ = cut_filters(aud["ch"])
            tag = ep_tag(c["ep"]["name"])
            rel = "%s/%s/%s_%s_%d.wav" % (a.era, kind.split(".")[0], kind.replace(".", "_"), tag, int(c["e"]))
            got = write_clip(c["ep"]["path"], 0, flt, c["e"] + 0.05, win - 0.05,
                             os.path.join(a.out, rel), fade_out=0.15)
            if got:
                manifest_add(a.out, {"era": a.era, "kind": kind, "file": rel, "source": c["ep"]["name"],
                                     "source_path": c["ep"]["path"], "t": round(c["e"] + 0.05, 2), "dur": round(got[0], 2),
                                     "after_line": c["text"], "metrics": {"level_db": round(level, 1), "dialogue_db": round(dlg, 1)},
                                     "status": "candidate"})


def e_len(c):
    return max(0.0, c["e"] - c["s"])


# ---------------------------------------------------------------------------------------------------------
# klaxon by spectrum (episodes without text subtitles)
# ---------------------------------------------------------------------------------------------------------

def find_alerts(x, ce=None, win=0.25, min_len=3.0):
    """Runs of a strong, moving narrowband tone (a klaxon). Returns [{t, dur, freq, freq_sd, share, level_db}]."""
    n = int(SR * win)
    hop = n // 2
    frames = []
    for i in range(0, len(x) - n, hop):
        f = x[i:i + n]
        sp = np.abs(np.fft.rfft(f * np.hanning(n))) ** 2
        fr = np.fft.rfftfreq(n, 1 / SR)
        band = (fr > 300) & (fr < 1800)
        tot = float(sp.sum()) + 1e-12
        k = int(np.argmax(np.where(band, sp, 0)))
        near = (fr > fr[k] - 60) & (fr < fr[k] + 60)
        frames.append((i / SR, fr[k], float(sp[near].sum() / tot), float(db(np.sqrt(np.mean(f ** 2))))))
    out, run_ = [], []
    for t, fq, sh, lv in frames + [(1e9, 0, 0, -99)]:
        if sh > 0.45 and lv > -40:
            run_.append((t, fq, sh, lv))
            continue
        if len(run_) * win / 2 >= min_len:
            fqs = np.array([r[1] for r in run_])
            out.append({"t": round(run_[0][0], 2), "dur": round(run_[-1][0] - run_[0][0] + win, 2), "freq": int(fqs.mean()),
                        "freq_sd": int(fqs.std()), "share": round(float(np.mean([r[2] for r in run_])), 2),
                        "level_db": round(float(np.mean([r[3] for r in run_])), 1)})
        run_ = []
    return out


def cmd_alert(a):
    idx = json.load(open(a.index))
    for ep in pick(idx, a.era, a.episodes):
        aud = ep["audio"][0]
        nc, ce = decode(ep["path"], aud["ch"])
        found = [f for f in find_alerts(nc) if f["freq_sd"] >= 40]
        print(ep["name"], len(found), "tonal runs", file=sys.stderr)
        flt, _ = cut_filters(aud["ch"])
        found.sort(key=lambda f: -(f["share"] * 10 + f["level_db"] / 10))
        for f in found[:a.per_key]:
            tag = ep_tag(ep["name"])
            rel = "%s/alert/alert_red_%s_%d.wav" % (a.era, tag, int(f["t"]))
            got = write_clip(ep["path"], 0, flt, f["t"], min(f["dur"], 6.0),
                             os.path.join(a.out, rel), fade_out=0.15)
            if got:
                manifest_add(a.out, {"era": a.era, "kind": "alert.red", "file": rel, "source": ep["name"], "source_path": ep["path"],
                                     "t": f["t"], "dur": round(got[0], 2), "metrics": f, "status": "candidate"})



# ---------------------------------------------------------------------------------------------------------
# cue table: manifest clips -> game keys (era twins), skipping clips Simon marked "no"
# ---------------------------------------------------------------------------------------------------------

# manifest kind -> game keys (each gets ".<era>")
SFX_KEYS = {
    "beam": ["weapon.Phaser", "weapon.PhaserBank", "weapon.PhaserArray", "weapon.generic.beam"],
    "launch": ["weapon.generic.torpedo", "weapon.PhotonTorpedo", "weapon.QuantumTorpedo"],
    "impact": ["sfx.explosion.small"],
    "hit": ["sfx.shield.hit"],
    "explosion": ["sfx.explosion.large"],
    "alert.red": ["sfx.alert.red"],
    "transporter": ["sfx.transporter"],
    "warp": ["sfx.warp.in", "sfx.warp.out"],
}
# voice_key -> game keys; {era} is appended
VOICE_KEYS = {
    "fed.ack": ["vo.combat.ack.fed"], "fed.engage": ["vo.combat.engage.fed"], "fed.fire": ["vo.combat.phaser.fed"],
    "fed.torpedo_away": ["vo.combat.torpedo.fed"], "fed.red_alert": ["vo.advisor.fleet.fed"],
    "fed.shields_up": ["vo.combat.shieldsup.fed"], "fed.battle_stations": ["vo.combat.encounter.fed"],
    "fed.evasive": ["vo.combat.evade.fed"], "fed.target_locked": ["vo.combat.target.fed"],
    "fed.shields_down": ["vo.combat.shields.50.fed", "vo.combat.shields.25.fed"], "fed.hull_breach": ["vo.combat.underattack.fed"],
    "fed.all_stop": ["vo.combat.allstop.fed"], "fed.hail": ["vo.combat.hail.fed"],
    "kli.ack": ["vo.combat.ack.kli"], "kli.fire": ["vo.combat.phaser.kli"], "kli.good_day": ["vo.combat.engage.kli"],
    "kli.qapla": ["vo.combat.kill.kli"], "kli.cloak": ["vo.combat.cloak.kli"],
}


def cmd_cues(a):
    table = {}
    for c in load_manifests(a.trek):
        if c.get("status") == "no":
            continue
        keys = VOICE_KEYS.get(c.get("voice_key", "")) if c["kind"] == "voice" else SFX_KEYS.get(c["kind"])
        for k in keys or []:
            lst = table.setdefault("%s.%s" % (k, c["era"]), [])
            if c["file"] not in lst:
                lst.append(c["file"])
    json.dump({"cues": table, "note": "private, generated by tools/mine_trek_audio.py cues; every clip is a candidate until "
               "Simon confirms it by ear (docs/AUDIO-LISTEN-SHEET.md)"}, open(os.path.join(a.trek, "cues.json"), "w"), indent=1)
    print(len(table), "era keys,", sum(len(v) for v in table.values()), "clips", file=sys.stderr)


# ---------------------------------------------------------------------------------------------------------
# listen sheet
# ---------------------------------------------------------------------------------------------------------

def load_manifests(trek):
    out = []
    for f in sorted(os.listdir(trek)):
        if re.match(r"manifest\.\w+\.json$", f):
            out += json.load(open(os.path.join(trek, f)))["clips"]
    return out


def mmss(t):
    return "%d:%02d.%d" % (int(t) // 60, int(t) % 60, int((t % 1) * 10))


def cmd_sheet(a):
    m = load_manifests(a.trek)
    trek_rel = a.trek_rel
    lines = ["# Audio listen sheet (candidates from Simon's Trek library)", "",
             "Generated by `remaster/tools/mine_trek_audio.py sheet`. Private: the clips live under `%s` (gitignored, never "
             "committed). **Every row is a candidate until Simon confirms it by ear.** Class names come from the spectrum and "
             "are guesses; voice rows carry the subtitle text. Mark each row `ok`, `no`, or a replacement timestamp." % trek_rel, "",
             "Tick column: `[ ]` untested. Source time is mm:ss.t in the named episode file.", ""]
    for era in ["tos", "tng", "ds9", "voy", "ent"]:
        rows = [c for c in m if c["era"] == era]
        if not rows:
            continue
        lines += ["## %s" % era.upper(), ""]
        kinds = sorted({c.get("voice_key", c["kind"]) if c["kind"] == "voice" else c["kind"] for c in rows})
        for k in kinds:
            sel = [c for c in rows if (c.get("voice_key", "") if c["kind"] == "voice" else c["kind"]) == k]
            lines += ["### %s" % (("voice " + k) if sel[0]["kind"] == "voice" else k), "",
                      "| ok | clip | source episode | time | length | notes |", "|---|---|---|---|---|---|"]
            for c in sorted(sel, key=lambda c: c["file"]):
                note = []
                if c["kind"] == "voice":
                    note.append('"%s"' % c["text"])
                    note.append("dry %+.0f dB" % c["dry_db"] if not c.get("stereo_source") else "stereo mix, centre not separable")
                elif "after_line" in c:
                    note.append('after "%s"' % c["after_line"])
                    note.append("level %s dB, centre %+.0f dB" % (c["metrics"]["level_db"], c["metrics"]["dialogue_db"]))
                elif "guess" in c:
                    mm = c["metrics"]
                    note.append("guess %s; peak %s dB, rise %s dB, centroid %s Hz, centre %+.0f dB" %
                                (c["guess"], mm["peak_db"], mm["rise_db"], mm["centroid_hz"], mm["dialogue_db"]))
                else:
                    mm = c.get("metrics", {})
                    note.append("tone %s Hz (sd %s), share %s" % (mm.get("freq"), mm.get("freq_sd"), mm.get("share")))
                lines.append("| [ ] | `%s/%s` | %s | %s | %.1f s | %s |" % (trek_rel, c["file"], c["source"], mmss(c["t"]), c["dur"], "; ".join(note)))
            lines.append("")
    lines += cue_index(a, m)
    open(a.out, "w", encoding="utf-8").write("\n".join(lines) + "\n")
    print(len(m), "clips on the sheet", file=sys.stderr)


def cue_index(a, manifest):
    """Per game key: which clip plays, from which episode at which time (Trek era twins), or which BOTF bank file."""
    by_file = {c["file"]: c for c in manifest}
    lines = ["## Cue index: what each game cue plays", "",
             "Era twin keys (`<key>.<era>`) are tried first, then the plain key (original BOTF bank), then a generated tone. "
             "Rows with a source episode are Trek clips; rows marked BOTF are the original game's own bank files (no timecode).", ""]
    trek_cues = os.path.join(a.trek, "cues.json")
    table = json.load(open(trek_cues))["cues"] if os.path.exists(trek_cues) else {}
    botf_path = os.path.join(os.path.dirname(os.path.normpath(a.trek)), "cues.json")
    botf = json.load(open(botf_path))["cues"] if os.path.exists(botf_path) else {}
    lines += ["| cue key | clip | source | time | note |", "|---|---|---|---|---|"]
    for key in sorted(set(table) | set(botf)):
        for f in table.get(key, []):
            c = by_file.get(f, {})
            note = '"%s"' % c["text"] if c.get("kind") == "voice" and "text" in c else c.get("kind", "")
            lines.append("| `%s` | `%s` | %s | %s | %s |" % (key, f, c.get("source", "?"), mmss(c["t"]) if "t" in c else "?", note))
        for f in botf.get(key, []):
            if key.startswith(("sfx.", "weapon.", "vo.combat", "vo.advisor.fleet")):
                lines.append("| `%s` | `%s` | BOTF bank | - | original game file |" % (key, f))
    lines.append("")
    return lines


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("index"); p.add_argument("--root", default="D:/TV Shows"); p.add_argument("--out", required=True)
    p = sub.add_parser("scan"); p.add_argument("--index", required=True); p.add_argument("--era", required=True)
    p.add_argument("--episodes", nargs="+", required=True); p.add_argument("--top", type=int, default=60); p.add_argument("--out", required=True)
    p = sub.add_parser("cut"); p.add_argument("--cand", required=True); p.add_argument("--out", required=True)
    p.add_argument("--per-class", type=int, default=2)
    p = sub.add_parser("cue"); p.add_argument("--index", required=True); p.add_argument("--era", required=True)
    p.add_argument("--episodes", nargs="*"); p.add_argument("--out", required=True); p.add_argument("--per-key", type=int, default=3)
    p.add_argument("--max-episodes", type=int, default=30)
    p = sub.add_parser("alert"); p.add_argument("--index", required=True); p.add_argument("--era", required=True)
    p.add_argument("--episodes", nargs="+", required=True); p.add_argument("--out", required=True); p.add_argument("--per-key", type=int, default=2)
    p = sub.add_parser("cues"); p.add_argument("--trek", required=True)
    p = sub.add_parser("sheet"); p.add_argument("--trek", required=True); p.add_argument("--out", required=True)
    p.add_argument("--trek-rel", default="LocalAssets/audio/trek")
    a = ap.parse_args()
    {"index": cmd_index, "scan": cmd_scan, "cut": cmd_cut, "cue": cmd_cue, "alert": cmd_alert, "cues": cmd_cues, "sheet": cmd_sheet}[a.cmd](a)


if __name__ == "__main__":
    main()
