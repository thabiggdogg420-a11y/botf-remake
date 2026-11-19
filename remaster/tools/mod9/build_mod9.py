#!/usr/bin/env python3
"""MOD-9 media (F4 round 3, 20 Nov 2026): the intro and race movies, the Dominion voice, the Borg cutscene lines and the
end-game speech texts. Offline pre-build step; everything it writes is private (Simon's disc and library), gitignored.

  python build_mod9.py --local <remaster/game/LocalAssets> --godot <remaster/godot> [--only movies,voice,speeches]

movies    BOTF.iso ENGLISH\\open.avi + INTRO.WAV -> godot/movies/open.ogv (the opening); H/K/R/C/F.avi -> <fac>.ogv (the race
          intros played when a campaign starts). Theora + Vorbis at 1024x384 (the disc's 8:3; a height that is not a multiple of 16 decodes with errors), letterboxed by the player.
voice     Clips cut by subtitle match from Simon's DS9 episodes and First Contact (centre channel of the 5.1 mix, so music and
          effects drop out), written to LocalAssets/audio/mod9/*.wav with LocalAssets/audio/mod9/cues.json (keys for the pack step,
          tools/pack_audio_web.py). Hails and viewscreen lines get a light comm band-pass.
speeches  LocalAssets/audio/mod9/speeches.json: {voice key: text} for the end-game speeches (the disc's EOG speeches from their
          Whisper transcripts, cleaned; the Dominion's from the subtitle text). pack_audio_web.py copies it to shared/speeches.json.

Each clip prints its duration, mean and peak level: the listen check. A clip under 1 s or with a mean under -40 dB is a failed cut.
"""
import argparse
import json
import os
import re
import subprocess
import sys

FFMPEG = os.environ.get("FFMPEG", "ffmpeg")
SEVENZ = os.environ.get("SEVENZ", "7z")
GODOT = os.environ.get("GODOT", r"C:\Tools\Godot\Godot_v4.5-stable_win64_console.exe")
RACE_MOVIES = {"H.avi": "fed", "K.AVI": "kli", "R.avi": "rom", "C.avi": "car", "F.avi": "fer"}
EOG_FAC = {"H": "fed", "K": "kli", "R": "rom", "C": "car", "F": "fer"}
EOG_KIND = {"Alli": "diplomatic", "Anni": "conquest", "Domi": "domination", "Loss": "defeat"}
FC = "D:/Movies/Star Trek/Star Trek 8 1996 First Contact"

# name, source (DS9 episode tag or FC), first-line regex, cue count, keys, comm filter
CLIPS = [
    ("dom_weyoun_endured", "S04E23", r"The Dominion has endured", 3,
     ["vo.diplomacy.war.dom", "vo.advisor.war.dom", "vo.victory.domination.dom"], True),
    ("dom_omet_dead", "S04E23", r"^I am First Omet", 10,
     ["vo.victory.conquest.dom", "vo.victory.win.dom", "vo.combat.encounter.dom"], False),
    ("dom_victory_is_life", "S05E15", r"^Victory is life!$", 2,
     ["vo.advisor.attack.dom", "vo.combat.encounter.dom", "vo.combat.kill.dom"], False),
    ("dom_obedience", "S06E02", r"obedience brings victory", 2,
     ["vo.combat.won.dom", "vo.combat.ack.dom"], False),
    ("dom_obey_order", "S06E02", r"assured that I will obey", 1,
     ["vo.combat.ack.dom", "vo.diplomacy.accept.dom"], False),
    ("dom_weyoun_servant", "S06E01", r"^The Founders are the masters", 2,
     ["vo.diplomacy.greet.dom", "vo.diplomacy.dom", "vo.advisor.contact.dom", "vo.victory.diplomatic.dom"], True),
    ("dom_founder_refuse", "S07E25E26", r"no intention of surrendering", 4,
     ["vo.diplomacy.refuse.dom", "vo.diplomacy.break.dom"], True),
    ("dom_founder_ceasefire", "S07E25E26", r"^If you will stand aside", 2,
     ["vo.victory.defeat.dom", "vo.diplomacy.peace.dom"], True),
    ("borg_hail", "FC", r"^We are the Borg\. Lower", 3, ["vo.borg.hail"], True),
    ("borg_locutus", "FC", r"^I am Locutus of Borg", 1, ["vo.borg.locutus"], True),
]


def run(cmd):
    return subprocess.run(cmd, capture_output=True, text=True, encoding="utf-8", errors="replace")


def srt_cues(path):
    t = open(path, encoding="utf-8-sig", errors="replace").read().replace("\r", "")
    out = []
    for b in re.split(r"\n\s*\n", t):
        ls = b.strip().split("\n")
        if len(ls) >= 3 and "-->" in ls[1]:
            a, z = [x.strip() for x in ls[1].split("-->")]
            text = re.sub(r"<[^>]+>", "", " ".join(ls[2:]))
            text = re.sub(r"^[A-Z' ]+:\s*", "", text).strip()   # speaker tags (JEM'HADAR:)
            out.append((secs(a), secs(z), text))
    return out


def secs(s):
    h, m, rest = s.replace(",", ".").split(":")
    return int(h) * 3600 + int(m) * 60 + float(rest)


def level(path):
    r = run([FFMPEG, "-v", "info", "-i", path, "-af", "volumedetect", "-f", "null", "-"])
    mean = re.search(r"mean_volume: (-?[\d.]+)", r.stderr)
    peak = re.search(r"max_volume: (-?[\d.]+)", r.stderr)
    dur = run(["ffprobe", "-v", "error", "-show_entries", "format=duration", "-of", "csv=p=0", path]).stdout.strip()
    return float(dur or 0), float(mean.group(1)) if mean else -99.0, float(peak.group(1)) if peak else -99.0


def build_movies(local, godot):
    iso = os.path.join(local, "incoming", "botf-iso", "BOTF.iso")
    src = os.path.join(local, "incoming", "movies-src")
    os.makedirs(src, exist_ok=True)
    names = ["open.avi", "INTRO.WAV"] + list(RACE_MOVIES)
    if not all(os.path.exists(os.path.join(src, n)) for n in names):
        r = run([SEVENZ, "e", "-y", "-o" + src, iso] + ["ENGLISH/" + n for n in names])
        if r.returncode != 0:
            print("7z failed:", r.stderr[-400:], file=sys.stderr)
            return 1
    out = os.path.join(godot, "movies")
    os.makedirs(out, exist_ok=True)
    jobs = [("open", [os.path.join(src, "open.avi"), os.path.join(src, "INTRO.WAV")])]
    jobs += [(fac, [os.path.join(src, n)]) for n, fac in RACE_MOVIES.items()]
    bad = 0
    work = os.path.join(src, "frames")
    for name, ins in jobs:
        dst = os.path.join(out, name + ".ogv")
        fdir = os.path.join(work, name)
        os.makedirs(fdir, exist_ok=True)
        wav = os.path.join(work, name + ".wav")
        r1 = run([FFMPEG, "-v", "error", "-y", "-i", ins[0], "-vf", "fps=24,scale=1024:384:flags=lanczos", os.path.join(fdir, "f%05d.png")])
        r2 = run([FFMPEG, "-v", "error", "-y", "-i", ins[-1], "-map", "0:a:0", "-ar", "44100", "-ac", "2", "-c:a", "pcm_s16le", wav])
        if r1.returncode != 0 or r2.returncode != 0:
            print("MOVIE FAIL", name, (r1.stderr + r2.stderr)[-300:])
            bad += 1
            continue
        # Godot's own Movie Maker writes the Theora (this ffmpeg's libtheora output does not decode cleanly).
        dst, fdir, wav = os.path.abspath(dst), os.path.abspath(fdir), os.path.abspath(wav)   # Godot resolves relative paths from the project
        tmp = os.path.join(os.path.abspath(work), name + ".video.ogv")
        before = os.path.getmtime(tmp) if os.path.exists(tmp) else 0
        proj = os.path.join(os.path.dirname(os.path.abspath(__file__)), "ogv_project")
        r = run([GODOT, "--path", proj, "--write-movie", tmp, "--fixed-fps", "24", "--resolution", "1024x384",
                 "--position", "-10000,-10000", "-s", "res://ogv_from_frames.gd", "--", fdir])
        if not os.path.exists(tmp) or os.path.getmtime(tmp) <= before or "OGV wrote" not in r.stdout:
            print("MOVIE FAIL", name, (r.stdout + r.stderr)[-400:])
            bad += 1
            continue
        # Godot's Theora video, the disc's sound muxed in as Vorbis (video stream copied, not re-encoded).
        r = run([FFMPEG, "-v", "error", "-y", "-i", tmp, "-i", wav, "-map", "0:v:0", "-map", "1:a:0", "-c:v", "copy",
                 "-c:a", "libvorbis", "-q:a", "4", dst])
        if r.returncode != 0:
            print("MOVIE FAIL mux", name, r.stderr[-300:])
            bad += 1
            continue
        errs = run([FFMPEG, "-v", "error", "-i", dst, "-f", "null", "-"]).stderr.count("error")
        d, mean, peak = level(dst)
        print("MOVIE %-5s %5.1f s  %5.1f MB  audio mean %.1f dB peak %.1f dB  decode errors %d" % (name, d, os.path.getsize(dst) / 1048576, mean, peak, errs))
        bad += 1 if errs else 0
    return bad


def build_voice(local):
    subs = os.path.join(local, "voice", "subs", "ds9")
    index = json.load(open(os.path.join(subs, "index.json"), encoding="utf-8"))["episodes"]
    out = os.path.join(local, "audio", "mod9")
    os.makedirs(out, exist_ok=True)
    cues, texts, bad = {}, {}, 0
    for name, src, pat, n, keys, comm in CLIPS:
        if src == "FC":
            srt, video = FC + ".eng.srt", FC + ".mp4"
        else:
            k = next((k for k in index if (" %s " % src) in k), None)
            if k is None:
                print("CLIP FAIL", name, "no episode", src)
                bad += 1
                continue
            srt, video = os.path.join(subs, k), index[k]["video"]
        cs = srt_cues(srt)
        i = next((i for i, c in enumerate(cs) if re.search(pat, c[2])), None)
        if i is None:
            print("CLIP FAIL", name, "no subtitle line", pat)
            bad += 1
            continue
        a, z = cs[i][0], cs[i + n - 1][1]
        text = " ".join(c[2] for c in cs[i:i + n]).replace("--", "-").replace("  ", " ").strip()
        dst = os.path.join(out, name + ".wav")
        af = "pan=mono|c0=FC,highpass=f=70"
        if comm:
            af += ",highpass=f=260,lowpass=f=5200"
        af += (",silenceremove=start_periods=1:start_threshold=-42dB:start_silence=0.08,areverse,"
               "silenceremove=start_periods=1:start_threshold=-42dB:start_silence=0.15,areverse")
        r = run([FFMPEG, "-v", "error", "-y", "-ss", "%.3f" % max(0.0, a - 0.2), "-to", "%.3f" % (z + 0.35), "-i", video,
                 "-map", "0:a:0", "-af", af, "-ar", "48000", "-ac", "1", dst])
        if r.returncode != 0 or not os.path.exists(dst):
            print("CLIP FAIL", name, r.stderr[-300:])
            bad += 1
            continue
        d, mean, peak = level(dst)
        ok = d >= 1.0 and mean > -40.0
        bad += 0 if ok else 1
        print("CLIP %-4s %-22s %5.2f s (subs %5.2f s)  mean %6.1f dB  peak %6.1f dB  | %s" % ("ok" if ok else "BAD", name, d, z - a, mean, peak, text))
        for key in keys:
            cues.setdefault(key, []).append(name + ".wav")
        texts[name] = text
    json.dump({"cues": cues, "texts": texts, "note": "private, tools/mod9/build_mod9.py"}, open(os.path.join(out, "cues.json"), "w"), indent=1)
    return bad


def clean(text):
    t = re.sub(r"\s+", " ", text).strip()
    t = re.sub(r"(\s+you)+\.?$", "", t)        # Whisper's trailing "you" on the silence after a speech
    for wrong, right in FIXES:
        t = t.replace(wrong, right)
    return t if t.endswith((".", "!", "?")) else t + "."


# Whisper's mishearings of Trek names in the EOG speeches (checked against the clips' wording by duration and context).
FIXES = [("foreign e-commerce authority", "Ferengi Commerce Authority"), ("The Ferengi Commerce", "The Ferengi Commerce"),
         ("slip of latin ", "slip of latinum "), ("Wrongless", "Romulus"), ("obsidian order", "Obsidian Order"),
         ("rules of acquisition", "Rules of Acquisition"), ("rule 242", "Rule 242"), ("single federation", "single Federation"),
         ("birth of the federation", "Birth of the Federation"), ("great federation", "great Federation"),
         ("yet will be no effort", "it will be no effort")]


def transcribe_missing(eog_dir):
    """Whisper (faster-whisper, CPU int8, small.en) over the EOG speeches and INTRO that transcripts.json lacks."""
    path = os.path.join(eog_dir, "transcripts.json")
    eog = json.load(open(path, encoding="utf-8")) if os.path.exists(path) else {}
    todo = [f for f in sorted(os.listdir(eog_dir)) if f.endswith(".ogg") and f not in eog]
    if not todo:
        return eog
    from faster_whisper import WhisperModel
    model = WhisperModel("small.en", device="cpu", compute_type="int8", cpu_threads=4)
    for f in todo:
        segs, info = model.transcribe(os.path.join(eog_dir, f), beam_size=5, vad_filter=True)
        segs = list(segs)
        eog[f] = {"text": " ".join(s.text.strip() for s in segs), "seconds": round(info.duration, 2),
                  "avgLogprob": round(sum(s.avg_logprob for s in segs) / max(1, len(segs)), 3)}
        print("WHISPER", f, eog[f]["text"][:100])
    json.dump(eog, open(path, "w", encoding="utf-8"), indent=1)
    return eog


def build_speeches(local):
    out = os.path.join(local, "audio", "mod9")
    os.makedirs(out, exist_ok=True)
    eog = transcribe_missing(os.path.join(local, "audio", "eog"))
    sp = {}
    for fname, v in eog.items():
        m = re.match(r"([HKRCF])EOG(\w{4})\.ogg$", fname)
        if not m:
            continue
        fac, kind = EOG_FAC[m.group(1)], EOG_KIND[m.group(2)]
        sp["vo.victory.%s.%s" % (kind, fac)] = {"text": clean(v["text"]), "seconds": v["seconds"], "speaker": "leader"}
        if kind == "conquest":
            sp["vo.victory.win.%s" % fac] = sp["vo.victory.%s.%s" % (kind, fac)]
    if "INTRO.ogg" in eog:
        sp["intro"] = {"text": clean(eog["INTRO.ogg"]["text"]), "seconds": eog["INTRO.ogg"]["seconds"], "speaker": "narrator"}
    voice = os.path.join(out, "cues.json")
    if os.path.exists(voice):
        vj = json.load(open(voice, encoding="utf-8"))
        speakers = {"dom_weyoun_endured": "Weyoun", "dom_omet_dead": "First Omet'iklan", "dom_weyoun_servant": "Weyoun",
                    "dom_founder_ceasefire": "The Female Changeling", "borg_hail": "The Collective", "borg_locutus": "Locutus"}
        for key, files in vj["cues"].items():
            if key.startswith("vo.victory.") or key.startswith("vo.borg."):
                name = files[0][:-4]
                d, _, _ = level(os.path.join(out, files[0]))
                sp[key] = {"text": clean(vj["texts"][name]), "seconds": round(d, 2), "speaker": speakers.get(name, "")}
    json.dump({"speeches": sp, "note": "private, tools/mod9/build_mod9.py"}, open(os.path.join(out, "speeches.json"), "w"), indent=1)
    for k in sorted(sp):
        print("SPEECH %-30s %5.1f s  %s" % (k, sp[k]["seconds"], sp[k]["text"][:90]))
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--local", required=True)
    ap.add_argument("--godot", required=True)
    ap.add_argument("--only", default="movies,voice,speeches")
    a = ap.parse_args()
    parts = a.only.split(",")
    bad = 0
    if "movies" in parts:
        bad += build_movies(a.local, a.godot)
    if "voice" in parts:
        bad += build_voice(a.local)
    if "speeches" in parts:
        bad += build_speeches(a.local)
    print("failures:", bad)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
