#!/usr/bin/env python3
"""Pulls the English text subtitle track (SubRip) out of each episode, for finding voice lines by their words.

  python extract_subtitles.py --src "D:\\TV Shows\\Star Trek Deep Space 9" --out <LocalAssets>/voice/subs/ds9

Writes <out>/<episode>.srt and <out>/index.json ({srt: source video, stream}). Image subtitles (PGS / DVD) are
skipped and listed, since they need transcription instead. Resumes: existing .srt files are kept. Private and
local (LocalAssets); never commit the output.
"""
import argparse
import json
import os
import subprocess

VIDEO = (".mkv", ".mp4", ".avi", ".m4v")


def english_text_stream(path):
    r = subprocess.run(["ffprobe", "-v", "error", "-select_streams", "s", "-show_entries",
                        "stream=index,codec_name:stream_tags=language,title", "-of", "json", path],
                       capture_output=True, text=True)
    try:
        streams = json.loads(r.stdout).get("streams", [])
    except ValueError:
        return None, []
    text = [s for s in streams if s.get("codec_name") in ("subrip", "ass", "mov_text", "webvtt")]
    english = [s for s in text if s.get("tags", {}).get("language", "eng") in ("eng", "en", "und")]
    # Prefer a plain track over one titled SDH / forced.
    english.sort(key=lambda s: any(w in s.get("tags", {}).get("title", "").lower() for w in ("sdh", "forced")))
    return (english[0]["index"] if english else None), [s.get("codec_name") for s in streams]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--src", required=True)
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    index_path = os.path.join(a.out, "index.json")
    index = json.load(open(index_path, encoding="utf-8")) if os.path.exists(index_path) else {"episodes": {}, "noText": []}
    videos = sorted(os.path.join(d, f) for d, _, fs in os.walk(a.src) for f in fs if f.lower().endswith(VIDEO))
    for n, video in enumerate(videos):
        name = os.path.splitext(os.path.basename(video))[0] + ".srt"
        target = os.path.join(a.out, name)
        if os.path.exists(target):
            continue
        stream, codecs = english_text_stream(video)
        if stream is None:
            if video not in index["noText"]:
                index["noText"].append(video)
            continue
        r = subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", video, "-map", "0:%d" % stream, "-f", "srt", target],
                           capture_output=True, text=True)
        if r.returncode == 0:
            index["episodes"][name] = {"video": video, "stream": stream}
        if n % 25 == 0:
            json.dump(index, open(index_path, "w", encoding="utf-8"), indent=1)
            print("%d/%d %s" % (n, len(videos), name), flush=True)
    json.dump(index, open(index_path, "w", encoding="utf-8"), indent=1)
    print("subtitles: %d episodes, %d without a text track -> %s" % (len(index["episodes"]), len(index["noText"]), a.out))


if __name__ == "__main__":
    main()
