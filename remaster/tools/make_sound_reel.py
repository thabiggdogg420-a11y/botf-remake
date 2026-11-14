#!/usr/bin/env python3
"""Phone-friendly sound reel: one mp4, each key's first clip in turn with 0.6 s of silence between, over a black frame captioned
with the key name. Reads <sheet>/build.json (build_trek_fx.py --sheet) and <sheet>/clips; writes <sheet>/sound-reel.mp4.
  python make_sound_reel.py <sheet_dir>
"""
import json
import os
import subprocess
import sys
import tempfile

FF = os.environ.get("FFMPEG", "ffmpeg")
FONT = r"C\:/Windows/Fonts/arial.ttf"


def main():
    d = sys.argv[1]
    b = json.load(open(os.path.join(d, "build.json")))
    tmp = tempfile.mkdtemp(prefix="reel_")
    segs = []
    for n, key in enumerate(sorted(b["cues"])):
        clip = os.path.join(d, "clips", b["cues"][key][0].split("/")[-1])
        out = os.path.join(tmp, "s%03d.mp4" % n)
        txt = key.replace(":", r"\:").replace("'", "")
        vf = ("drawtext=fontfile='%s':text='%s':fontcolor=white:fontsize=34:x=(w-text_w)/2:y=(h-text_h)/2,"
              "drawtext=fontfile='%s':text='%s':fontcolor=0x999999:fontsize=20:x=(w-text_w)/2:y=h*0.62" % (
                  FONT, txt, FONT, os.path.basename(clip)))
        r = subprocess.run([FF, "-v", "error", "-y", "-i", clip, "-f", "lavfi", "-i", "color=c=black:s=854x480:r=10", "-af", "apad=pad_dur=0.6",
                            "-shortest", "-vf", vf, "-c:v", "libx264", "-preset", "veryfast", "-crf", "30", "-pix_fmt", "yuv420p",
                            "-c:a", "aac", "-b:a", "96k", "-ar", "32000", "-ac", "1", out], capture_output=True, text=True)
        if r.returncode:
            print("fail", key, r.stderr[:200])
            continue
        segs.append(out)
    lst = os.path.join(tmp, "list.txt")
    open(lst, "w").write("".join("file '%s'\n" % s.replace("\\", "/") for s in segs))
    final = os.path.join(d, "sound-reel.mp4")
    subprocess.run([FF, "-v", "error", "-y", "-f", "concat", "-safe", "0", "-i", lst, "-c", "copy", "-movflags", "+faststart", final])
    print(final, os.path.getsize(final) // 1024, "KB", len(segs), "segments")


if __name__ == "__main__":
    main()
