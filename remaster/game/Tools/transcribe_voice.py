#!/usr/bin/env python3
"""Transcribes local voice clips with faster-whisper so they can be mapped to game cues (vo.* keys).

  python transcribe_voice.py --dir <folder of *.48k.wav> [--out transcripts.json] [--model small.en] [--device auto]

Writes {file: {"text", "seconds", "avgLogprob"}} and resumes: files already in --out are skipped. Local and private
like the audio itself (LocalAssets); never commit the output.
"""
import argparse
import glob
import json
import os
import sys


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--dir", required=True)
    ap.add_argument("--out", default=None)
    ap.add_argument("--model", default="small.en")
    ap.add_argument("--device", default="auto", choices=["auto", "cuda", "cpu"])
    ap.add_argument("--pattern", default="*.48k.wav")
    a = ap.parse_args()
    # CUDA runtime from the pip wheels (nvidia-cublas-cu12, nvidia-cudnn-cu12), when installed: CTranslate2 loads
    # cublas64_12.dll / cudnn*.dll by name, so their bin folders go on the DLL search path and PATH.
    try:
        import nvidia.cublas, nvidia.cudnn
        for pkg in (nvidia.cublas, nvidia.cudnn):
            bin_dir = os.path.join(pkg.__path__[0], "bin")
            if os.path.isdir(bin_dir):
                os.add_dll_directory(bin_dir)
                os.environ["PATH"] = bin_dir + os.pathsep + os.environ.get("PATH", "")
    except ImportError:
        pass
    from faster_whisper import WhisperModel
    out = a.out or os.path.join(a.dir, "transcripts.json")
    done = json.load(open(out, encoding="utf-8")) if os.path.exists(out) else {}
    model = None
    for device, compute in ([("cuda", "float16"), ("cpu", "int8")] if a.device == "auto" else [(a.device, "float16" if a.device == "cuda" else "int8")]):
        try:
            model = WhisperModel(a.model, device=device, compute_type=compute)
            print("model %s on %s" % (a.model, device), flush=True)
            break
        except Exception as e:  # no CUDA runtime: fall back to the CPU
            print("cannot use %s: %s" % (device, e), flush=True)
    if model is None:
        sys.exit("no usable device")
    files = sorted(glob.glob(os.path.join(a.dir, a.pattern)))
    for n, path in enumerate(files):
        key = os.path.basename(path)
        if key in done:
            continue
        segments, info = model.transcribe(path, beam_size=5, vad_filter=False, condition_on_previous_text=False)
        segs = list(segments)
        done[key] = {"text": " ".join(s.text.strip() for s in segs).strip(), "seconds": round(info.duration, 2),
                     "avgLogprob": round(sum(s.avg_logprob for s in segs) / len(segs), 3) if segs else None}
        if n % 50 == 0:
            json.dump(done, open(out, "w", encoding="utf-8"), indent=1)
            print("%d/%d %s: %s" % (n, len(files), key, done[key]["text"][:70]), flush=True)
    json.dump(done, open(out, "w", encoding="utf-8"), indent=1)
    print("transcribed %d files -> %s" % (len(done), out))


if __name__ == "__main__":
    main()
