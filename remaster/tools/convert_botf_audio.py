#!/usr/bin/env python3
"""Converts the extracted BOTF banks for the Godot client: voice and music to ogg (libvorbis), sfx to 22.05 kHz wav.
  python convert_botf_audio.py <ffmpeg> <botf-original/audio> <LocalAssets/audio>   (private and local only)
"""
import os, sys, subprocess, json, shutil, concurrent.futures as cf
ffmpeg = sys.argv[1]; src = sys.argv[2]; dst = sys.argv[3]
jobs = []
for bank, sub, ext, args in [("english", "voice", "ogg", ["-ac", "1", "-c:a", "libvorbis", "-q:a", "3"]),
                             ("music", "music", "ogg", ["-c:a", "libvorbis", "-q:a", "4"]),
                             ("sfx", "sfx", "wav", ["-ar", "22050", "-ac", "1", "-c:a", "pcm_s16le"])]:
    d = os.path.join(dst, sub); os.makedirs(d, exist_ok=True)
    for f in sorted(os.listdir(os.path.join(src, bank))):
        if f.endswith(".wav") and ".48k." not in f:
            jobs.append((os.path.join(src, bank, f), os.path.join(d, f[:-4] + "." + ext), args))
def run(j):
    i, o, a = j
    if os.path.exists(o): return 0
    r = subprocess.run([ffmpeg, "-v", "error", "-y", "-threads", "1", "-i", i] + a + [o], capture_output=True, text=True)
    return r.returncode
with cf.ThreadPoolExecutor(4) as ex:
    res = list(ex.map(run, jobs))
print(len(jobs), "jobs,", sum(1 for r in res if r), "failed")
