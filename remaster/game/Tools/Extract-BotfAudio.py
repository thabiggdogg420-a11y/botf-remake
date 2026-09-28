#!/usr/bin/env python3
"""Extract sound effects, music and voice lines from the user's OWN Birth of the Federation install.

Output is private and local only (default: remaster/game/LocalAssets/botf-original/audio). Never commit it.

Format (from the Ultimate Editor sources, gitlab.com/stbotf/ultimate-editor, source/ue/edit/snd and
source/ue/audio/Wave.java; re-implemented here, no code copied):

  sfx.snd, music.snd, english.snd, german.snd
    uint32 LE  map offset (from file start)
    ...        concatenated audio payloads
    at map offset, until EOF: 16-byte entries
        uint32 LE address   payload offset
        uint32 LE size      payload bytes
        uint32 LE frames    sample frames (0 for the two trailing bookkeeping entries)
        uint32 LE unknown
    The Ultimate Editor treats a trailing entry with frames == 0 as its own "mods" / "description"
    bookkeeping record, not a sound; those are skipped here.

  Payload encodings (no RIFF header stored for voice/music):
    sfx.snd      PCM 16-bit mono 22050 Hz (payload may already be a RIFF file; kept as-is if so)
    english.snd  IMA ADPCM (WAVE format 0x11) mono 22050 Hz, block 512, 1017 samples/block
    music.snd    IMA ADPCM stereo 22050 Hz, block 1024, 1017 samples/block

Voice entries are grouped by race and conversation group through converse.bin inside stbof.res; this
script writes the raw index. AUDIO-PLAN.md explains how the categories are assigned.

Usage:
  python Extract-BotfAudio.py --botf "C:\\Games\\BOTF" [--out DIR] [--convert] [--ffmpeg PATH]

--convert additionally writes 48 kHz 16-bit PCM copies (<name>.48k.wav) with ffmpeg, which the
importer and Unreal expect. The format header is untested against every retail/GOG/AIO variant; if a
file does not play, report its index.json entry.
"""
import argparse
import hashlib
import json
import os
import shutil
import struct
import subprocess
import sys

FORMATS = {
    # name: (kind, wFormatTag, channels, rate, bits, blockAlign, samplesPerBlock)
    "sfx.snd": ("sfx", 1, 1, 22050, 16, 2, None),
    "music.snd": ("music", 0x11, 2, 22050, 4, 1024, 1017),
    "english.snd": ("voice", 0x11, 1, 22050, 4, 512, 1017),
    "german.snd": ("voice", 0x11, 1, 22050, 4, 512, 1017),
}


def read_entries(data):
    if len(data) < 4:
        raise ValueError("file too short")
    map_off = struct.unpack_from("<I", data, 0)[0]
    if map_off < 4 or map_off > len(data):
        raise ValueError("map offset %d outside file (%d bytes)" % (map_off, len(data)))
    entries = []
    pos = map_off
    while pos + 16 <= len(data):
        addr, size, frames, unknown = struct.unpack_from("<IIII", data, pos)
        pos += 16
        remaining = len(data) - pos
        if frames == 0 and remaining <= 16:
            continue  # editor bookkeeping (mods / description entry)
        if addr + size > len(data):
            raise ValueError("entry %d points beyond EOF" % len(entries))
        entries.append((addr, size, frames, unknown))
    return entries


def wav_header(fmt_tag, channels, rate, bits, block_align, spb, data_len, frames):
    if fmt_tag == 1:
        fmt = struct.pack("<HHIIHH", 1, channels, rate, rate * block_align, block_align, bits)
        chunks = b"fmt " + struct.pack("<I", len(fmt)) + fmt
    else:
        byte_rate = rate * block_align // spb
        fmt = struct.pack("<HHIIHHHH", fmt_tag, channels, rate, byte_rate, block_align, bits, 2, spb)
        chunks = b"fmt " + struct.pack("<I", len(fmt)) + fmt
        if frames:
            chunks += b"fact" + struct.pack("<II", 4, frames)
    body = b"WAVE" + chunks + b"data" + struct.pack("<I", data_len)
    return b"RIFF" + struct.pack("<I", len(body) + data_len + (data_len & 1)) + body


def extract(path, out_dir, ffmpeg):
    name = os.path.basename(path).lower()
    kind, tag, ch, rate, bits, align, spb = FORMATS[name]
    stem = name.split(".")[0]
    with open(path, "rb") as f:
        data = f.read()
    entries = read_entries(data)
    target = os.path.join(out_dir, stem)
    os.makedirs(target, exist_ok=True)
    index = []
    for i, (addr, size, frames, unknown) in enumerate(entries):
        payload = data[addr:addr + size]
        if payload[:4] == b"RIFF":
            blob = payload
        else:
            blob = wav_header(tag, ch, rate, bits, align, spb, len(payload), frames) + payload + (b"\0" if len(payload) & 1 else b"")
        fn = "%s_%04d.wav" % (stem, i)
        with open(os.path.join(target, fn), "wb") as w:
            w.write(blob)
        rec = {"index": i, "file": fn, "kind": kind, "offset": addr, "size": size, "frames": frames,
               "unknown": unknown, "sha256": hashlib.sha256(blob).hexdigest()}
        if ffmpeg:
            out48 = os.path.join(target, "%s_%04d.48k.wav" % (stem, i))
            r = subprocess.run([ffmpeg, "-v", "error", "-y", "-i", os.path.join(target, fn), "-ar", "48000",
                                "-acodec", "pcm_s16le", out48], capture_output=True, text=True)
            rec["converted"] = os.path.basename(out48) if r.returncode == 0 else None
            if r.returncode != 0:
                rec["convertError"] = r.stderr.strip()[:300]
        index.append(rec)
    with open(os.path.join(target, "index.json"), "w", encoding="utf-8") as w:
        json.dump({"source": path, "sourceSha256": hashlib.sha256(data).hexdigest(), "entries": index}, w, indent=1)
    return len(index)


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--botf", required=True, help="BOTF install folder containing trek.exe and the .snd files")
    ap.add_argument("--out", default=os.path.normpath(os.path.join(here, "..", "LocalAssets", "botf-original", "audio")))
    ap.add_argument("--convert", action="store_true", help="also write 48 kHz PCM copies with ffmpeg")
    ap.add_argument("--ffmpeg", default="ffmpeg")
    a = ap.parse_args()
    ffmpeg = None
    if a.convert:
        ffmpeg = shutil.which(a.ffmpeg) or (a.ffmpeg if os.path.isfile(a.ffmpeg) else None)
        if not ffmpeg:
            sys.exit("ffmpeg not found; install it (winget install Gyan.FFmpeg) or pass --ffmpeg")
    found = 0
    for fn in os.listdir(a.botf):
        if fn.lower() in FORMATS:
            n = extract(os.path.join(a.botf, fn), a.out, ffmpeg)
            print("%-12s %5d sounds -> %s" % (fn, n, os.path.join(a.out, fn.lower().split(".")[0])))
            found += 1
    if not found:
        sys.exit("No sfx.snd / music.snd / english.snd / german.snd in %s" % a.botf)


if __name__ == "__main__":
    main()
