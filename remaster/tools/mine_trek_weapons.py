#!/usr/bin/env python3
"""M4-AUDIO: finds clean weapon, hit, explosion and warp sounds in Simon's own Trek episodes and films (private, read only,
never committed, never downloaded). Offline step: the game runtime never reads these disks.

  python mine_trek_weapons.py find   <pattern> ...                    list matching files with their audio layout
  python mine_trek_weapons.py events <pattern> ... --out events.json   isolated sound events with features
  python mine_trek_weapons.py sheet  <events.json> --out sheet.png     contact sheet of spectrograms to judge by eye
  python mine_trek_weapons.py cut    <picks.json>                     cut, trim, fade, loudness-match the picks

Method. 5.1 mixes are folded to "no centre" (front L/R and the surrounds; dialogue lives in the centre channel). A stereo mix
has no centre, so moments under a subtitle line are skipped instead. A moment is an onset of 12 dB or more over the half second
before it. It is "clean" when that half second was quiet and nothing sustained (music) sits under it. Every pick is a
CANDIDATE that Simon confirms by ear on the listen sheet (m4-audio-captures/listen-sheet.html).
Picks file: {"clips": [{"key": "...", "src": "<file pattern>", "t": sec, "dur": sec, "gain_db": 0, "note": "..."}]}.
"""
import argparse
import json
import os
import re
import subprocess
import sys
import wave

import numpy as np

FFMPEG = os.environ.get("FFMPEG", "ffmpeg")
FFPROBE = os.environ.get("FFPROBE", "ffprobe")
SR = 22050
OUT_SR = 32000
CACHE = os.environ.get("TREKFX_CACHE", r"C:\Users\BiggDogg\Documents\ChatGPT\m4-audio-captures\cache")
ROOTS = [r"D:\TV Shows\Star Trek Deep Space 9", r"D:\TV Shows\Star Trek TNG", r"D:\TV Shows\Star Trek.TOS",
         r"D:\TV Shows\Star Trek - Voyager (1080p AI Upscale DVD x265 HEVC 10bit AC3 5.1 Vertag)",
         r"D:\TV Shows\Star Trek Enterprise Season", r"D:\Movies\Star Trek"]
VIDEO_EXT = (".mkv", ".mp4", ".avi")
SKIP_DIRS = ("Extras", "Featurettes", "Special", "zExtras", "Bonus", "Preview")


def run(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, **kw)


_FILES = None


def all_files():
    global _FILES
    if _FILES is None:
        _FILES = []
        for root in ROOTS:
            for dp, dn, fn in os.walk(root):
                if any(x in dp for x in SKIP_DIRS):
                    continue
                for f in sorted(fn):
                    if f.lower().endswith(VIDEO_EXT) and "bonus" not in f.lower():
                        _FILES.append(os.path.join(dp, f))
    return _FILES


def match(patterns):
    out = []
    for p in patterns:
        rx = re.compile(p, re.I)
        out += [f for f in all_files() if rx.search(os.path.basename(f)) and f not in out]
    return out


def probe(path):
    r = run([FFPROBE, "-v", "error", "-show_entries",
             "stream=index,codec_type,codec_name,channels,channel_layout:stream_tags=language",
             "-show_entries", "format=duration", "-of", "json", path], text=True, encoding="utf-8", errors="replace")
    try:
        return json.loads(r.stdout)
    except Exception:
        return {}


def era_of(path):
    return ("ds9" if "Deep Space" in path else "tng" if "TNG" in path else "tos" if "TOS" in path else
            "voy" if "Voyager" in path else "ent" if "Enterprise" in path else "mov")


def tag(path):
    b = os.path.basename(path)
    m = re.search(r"S(\d{1,2})E(\d{2})", b, re.I)
    if m:
        return "%s_s%02de%02d" % (era_of(path), int(m.group(1)), int(m.group(2)))
    return "mov_" + re.sub(r"[^a-z0-9]+", "-", b.lower()).strip("-")[:40]


def decode_one(path, flt, sr=SR, start=None, dur=None):
    pre = []
    if start is not None:
        pre += ["-ss", "%.3f" % max(0, start)]
    if dur is not None:
        pre += ["-t", "%.3f" % dur]
    r = run([FFMPEG, "-v", "error"] + pre + ["-i", path, "-map", "0:a:0", "-af", flt, "-ar", str(sr), "-f", "f32le", "-"])
    return np.frombuffer(r.stdout, dtype=np.float32)


def folds(ch):
    """name -> pan filter. nc: no centre (effects, music); ce: centre (dialogue)."""
    if ch >= 6:
        return {"nc": "pan=mono|c0=0.5*c0+0.5*c1+0.35*c4+0.35*c5", "ce": "pan=mono|c0=c2"}
    return {"nc": "pan=mono|c0=0.5*c0+0.5*c1", "ce": "pan=mono|c0=0.5*c0+0.5*c1"}


def channels(path):
    aud = [s for s in probe(path).get("streams", []) if s.get("codec_type") == "audio"]
    return int(aud[0].get("channels", 2)) if aud else 2


def load(path):
    cp = os.path.join(CACHE, tag(path) + ".npz")
    if os.path.exists(cp):
        z = np.load(cp)
        return {k: z[k] for k in ("nc", "ce")}, int(z["ch"])
    ch = channels(path)
    d = {k: decode_one(path, f) for k, f in folds(ch).items()}
    n = min(len(v) for v in d.values())
    d = {k: v[:n] for k, v in d.items()}
    os.makedirs(CACHE, exist_ok=True)
    np.savez(cp, ch=ch, **d)
    return d, ch


def srt_for(path):
    """Subtitle lines [(t0, t1, text)] from a sidecar srt or the first text subtitle stream; [] when none."""
    base = os.path.splitext(path)[0]
    d = os.path.dirname(path)
    stem = os.path.splitext(os.path.basename(path))[0].lower()
    txt = None
    for f in os.listdir(d):
        if f.lower().endswith(".srt") and f.lower().startswith(stem[:18]):
            txt = open(os.path.join(d, f), encoding="utf-8", errors="replace").read()
            break
    if txt is None:
        subs = [s for s in probe(path).get("streams", []) if s.get("codec_type") == "subtitle"
                and s.get("codec_name") in ("subrip", "ass", "mov_text")]
        if subs:
            r = run([FFMPEG, "-v", "error", "-i", path, "-map", "0:%d" % subs[0]["index"], "-f", "srt", "-"])
            txt = r.stdout.decode("utf-8", "replace")
    if not txt:
        return []
    out = []
    for blk in re.split(r"\r?\n\r?\n", txt):
        m = re.search(r"(\d+):(\d+):(\d+)[,.](\d+)\s*-->\s*(\d+):(\d+):(\d+)[,.](\d+)", blk)
        if not m:
            continue
        g = [int(x) for x in m.groups()]
        out.append((g[0] * 3600 + g[1] * 60 + g[2] + g[3] / 1000, g[4] * 3600 + g[5] * 60 + g[6] + g[7] / 1000,
                    re.sub(r"<[^>]+>", "", blk[m.end():].strip()).replace("\n", " ")))
    return out


def db(x, floor=1e-7):
    return 20 * np.log10(np.maximum(x, floor))


def frame_rms(x, hop):
    n = len(x) // hop
    return np.sqrt(np.mean(x[:n * hop].reshape(n, hop) ** 2, axis=1)) if n else np.zeros(0)


def band_shares(seg):
    """Energy shares: <200 Hz, 200-1000, 1000-4000, >4000, and the spectral centroid (Hz)."""
    if len(seg) < 512:
        return [0, 0, 0, 0], 0
    sp = np.abs(np.fft.rfft(seg * np.hanning(len(seg)))) ** 2 + 1e-12
    fr = np.fft.rfftfreq(len(seg), 1 / SR)
    tot = float(np.sum(sp))
    sh = [float(np.sum(sp[(fr >= lo) & (fr < hi)]) / tot) for lo, hi in ((0, 200), (200, 1000), (1000, 4000), (4000, 99999))]
    return sh, float(np.sum(fr * sp) / tot)


def band_env(x, hop=220, nfft=1024):
    """Per 10 ms frame energy (dB) in four bands: all, <400 Hz, 400-2000, 2000-9000."""
    w = np.hanning(nfft).astype(np.float32)
    fr = np.fft.rfftfreq(nfft, 1 / SR)
    masks = [(fr >= 40) & (fr < 400), (fr >= 400) & (fr < 2000), (fr >= 2000) & (fr < 9000)]
    n = max(0, (len(x) - nfft) // hop)
    out = np.zeros((4, n), np.float32)
    step = 4096
    for a in range(0, n, step):
        b = min(n, a + step)
        idx = np.arange(a, b)[:, None] * hop + np.arange(nfft)[None, :]
        sp = np.abs(np.fft.rfft(x[idx] * w, axis=1)) ** 2
        for k, m in enumerate(masks):
            out[k + 1, a:b] = sp[:, m].sum(axis=1)
    out[0] = out[1:].sum(axis=0)
    return 10 * np.log10(out + 1e-9)


def find_events(path, rise_db=12.0):
    """Onsets in the no-centre fold, in the whole signal or the 2-9 kHz band (weapons are bright, music is not)."""
    d, ch = load(path)
    nc, ce = d["nc"], d["ce"]
    hop = 220
    be = band_env(nc, hop)
    ec = band_env(ce, hop)[0] if ch >= 6 else np.zeros(be.shape[1])
    n = min(be.shape[1], len(ec))
    be, ec = be[:, :n], ec[:n]
    fps = SR / hop
    subs = srt_for(path)
    spoken = np.zeros(n, bool)
    for t0, t1, tx in subs:
        if not tx.startswith("(") and not tx.startswith("["):
            spoken[max(0, int((t0 - 0.25) * fps)):min(n, int((t1 + 0.25) * fps))] = True
    all_db, hf = be[0], be[3]
    floor_all = float(np.percentile(all_db, 50))
    floor_hf = float(np.percentile(hf, 60))
    pre_n, i, last, out = int(0.6 * fps), int(0.6 * fps), -10.0, []
    while i < n - int(2 * fps):
        up_all = all_db[i] - float(np.median(all_db[i - pre_n:i - 2])) if all_db[i] > floor_all else 0
        up_hf = hf[i] - float(np.median(hf[i - pre_n:i - 2])) if hf[i] > floor_hf else 0
        if (up_all < rise_db and up_hf < rise_db) or all_db[i] < all_db[i - 1] - 0.5 or i / fps - last < 0.6:
            i += 1
            continue
        j = i + int(np.argmax(all_db[i:i + int(0.3 * fps)]))
        pk = float(all_db[j])
        tail = all_db[j:j + int(8 * fps)]
        below = np.where(tail < pk - 20)[0]
        dur = (below[0] if len(below) else len(tail)) / fps
        pre_all = float(np.median(all_db[i - pre_n:i - 2]))
        pre_max = float(np.max(all_db[i - pre_n:i - 2]))
        k1 = j + int(max(dur, 0.3) * fps)
        speech = 0.0 if spoken[i:j + int(min(dur, 3.0) * fps)].any() else -40.0 + max(0.0, float(np.max(ec[i:k1])) - pk - 22.0)
        seg = nc[i * hop:i * hop + int(min(dur + 0.05, 3.0) * SR)]
        sh, cen = band_shares(seg)
        w0, w1 = max(0, i - int(1.5 * fps)), min(n, i + int(3 * fps))
        snr3 = pk - float(np.percentile(all_db[w0:w1], 10))
        snr_hf = float(hf[j]) - float(np.percentile(hf[w0:w1], 25))
        out.append({"t": round(i / fps, 2), "peak": round(pk, 1), "rise_all": round(pk - pre_all, 1), "gap_max": round(pk - pre_max, 1),
                    "snr3": round(snr3, 1), "snr_hf": round(snr_hf, 1),
                    "rise_hf": round(float(up_hf), 1), "dur": round(dur, 2), "centroid": int(cen), "shares": [round(s, 2) for s in sh],
                    "speech": round(speech, 1), "next_gap": 0.0})
        last = i / fps
        i += int(0.3 * fps)
    for a, b in zip(out, out[1:]):
        a["next_gap"] = round(b["t"] - a["t"], 2)
    for e in out:
        e["subs"] = [s[2] for s in subs if s[0] - 6 < e["t"] < s[1] + 3][:2] if subs else []
    return out


def spectrogram(x, sr=SR, nfft=1024, hop=256, fmax=9000):
    w = np.hanning(nfft)
    n = max(0, (len(x) - nfft) // hop)
    if n == 0:
        return np.zeros((10, 10))
    fr = np.stack([x[i * hop:i * hop + nfft] * w for i in range(n)])
    sp = np.abs(np.fft.rfft(fr, axis=1)).T
    k = int(fmax / (sr / 2) * sp.shape[0])
    s = 20 * np.log10(sp[:k] + 1e-6)
    return s


def spec_image(x, width=300, height=110, dyn=60.0):
    """Greyscale-to-heat PIL image of a log spectrogram (low frequencies at the bottom)."""
    from PIL import Image
    s = spectrogram(x)
    top = s.max()
    s = np.clip((s - (top - dyn)) / dyn, 0, 1)[::-1]
    img = Image.fromarray((s * 255).astype(np.uint8)).resize((width, height))
    return img.convert("RGB")


def cmd_find(a):
    for f in match(a.patterns):
        aud = [s for s in probe(f).get("streams", []) if s.get("codec_type") == "audio"]
        print("%s | %s" % (f, ",".join("%s/%s" % (s.get("codec_name"), s.get("channel_layout")) for s in aud)))


def cmd_events(a):
    res = []
    for f in match(a.patterns):
        ev = find_events(f)
        print("%s: %d events" % (os.path.basename(f), len(ev)), file=sys.stderr)
        res.append({"file": f, "tag": tag(f), "events": ev})
    json.dump(res, open(a.out, "w"), indent=1)


def clip_samples(path, t, dur, ch, mode="nc", sr=OUT_SR):
    flt = folds(ch)["nc"] if mode == "nc" else {"fr": "pan=mono|c0=0.5*c0+0.5*c1" if ch >= 6 else "pan=mono|c0=0.5*c0+0.5*c1",
                                                 "full": "pan=mono|c0=0.35*c0+0.35*c1+0.2*c4+0.2*c5+0.3*c3" if ch >= 6 else
                                                 "pan=mono|c0=0.5*c0+0.5*c1"}[mode]
    return decode_one(path, flt, sr, t, dur)


def write_wav(path, x, sr=OUT_SR):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(sr)
        w.writeframes((np.clip(x, -1, 1) * 32767).astype("<i2").tobytes())


def shape_clip(x, sr, target_peak_db=-3.0, fade_in=0.004, fade_out=0.05, trim_db=-45.0, max_len=None):
    """High-pass the rumble below 40 Hz, trim leading and trailing near-silence, fade, peak-match to -3 dBFS."""
    from numpy.fft import rfft, irfft
    X = rfft(x)
    fr = np.fft.rfftfreq(len(x), 1 / sr)
    X[fr < 40] = 0
    x = irfft(X, len(x))
    pk = float(np.max(np.abs(x)))
    if pk < 1e-5:
        return None
    k = max(1, int(0.01 * sr))
    env = np.convolve(np.abs(x), np.ones(k) / k, mode="same")
    live = np.where(env > pk * 10 ** (trim_db / 20))[0]
    if not len(live):
        return None
    a, b = max(0, live[0] - int(0.005 * sr)), min(len(x), live[-1] + int(0.02 * sr))
    x = x[a:b]
    if max_len and len(x) > max_len * sr:
        x = x[:int(max_len * sr)]
    x = x * (10 ** (target_peak_db / 20) / max(float(np.max(np.abs(x))), 1e-9))
    fi, fo = min(int(fade_in * sr), len(x) // 2), min(int(fade_out * sr), len(x) // 2)
    x[:fi] *= np.linspace(0, 1, fi)
    x[-fo:] *= np.linspace(1, 0, fo)
    return x


def cmd_cut(a):
    spec = json.load(open(a.patterns[0]))
    for c in spec["clips"]:
        f = match([c["src"]])[0]
        ch = channels(f)
        x = clip_samples(f, c["t"], c["dur"], ch, c.get("mode", "nc"))
        y = shape_clip(x, OUT_SR, max_len=c.get("max_len"))
        if y is None:
            print("EMPTY", c)
            continue
        write_wav(os.path.join(spec["out"], c["file"]), y)
        print("cut", c["file"], "%.2fs" % (len(y) / OUT_SR))


def frame_at(path, t, w=150):
    """One video frame at t seconds as a PIL image (None when ffmpeg cannot read it)."""
    import io
    from PIL import Image
    r = run([FFMPEG, "-v", "error", "-ss", "%.2f" % max(0, t), "-i", path, "-frames:v", "1", "-vf", "scale=%d:-2" % w,
             "-f", "image2pipe", "-vcodec", "png", "-"])
    try:
        return Image.open(io.BytesIO(r.stdout)).convert("RGB")
    except Exception:
        return None


def cmd_sheet(a):
    """Contact sheet: per event a spectrogram and two video frames (at the onset and 0.7 s later)."""
    from PIL import Image, ImageDraw
    data = json.load(open(a.patterns[0]))
    items = [(ep, e) for ep in data for e in ep["events"]]
    cols, w, sh, fh = 4, 300, 70, 84
    cell = 24 + sh + fh
    rows = (len(items) + cols - 1) // cols
    sheet = Image.new("RGB", (cols * w, rows * cell), (20, 20, 20))
    dr = ImageDraw.Draw(sheet)
    for n, (ep, e) in enumerate(items):
        d, ch = load(ep["file"])
        t0 = max(0, e["t"] - 0.5)
        seg = d["nc"][int(t0 * SR):int((t0 + max(2.0, min(e["dur"] + 1.0, 4.0))) * SR)]
        x, y = (n % cols) * w, (n // cols) * cell
        sheet.paste(spec_image(seg, w, sh), (x, y + 24))
        for k, dt in enumerate((0.05, 0.7)):
            fr = frame_at(ep["file"], e["t"] + dt)
            if fr is not None:
                sheet.paste(fr.resize((150, fh)), (x + k * 150, y + 24 + sh))
        dr.text((x + 2, y + 1), "%d %s %.1f" % (n, ep["tag"], e["t"]), fill=(255, 255, 0))
        dr.text((x + 2, y + 12), "pk%.0f r%.0f/%.0f g%.0f d%.1f c%d" % (e["peak"], e["rise_all"], e["rise_hf"], e["gap_max"], e["dur"], e["centroid"]), fill=(180, 220, 255))
    sheet.save(a.out)


def big_spec(path, t0, t1, out, fmax=10000):
    """Spectrogram of the no-centre fold (top) and the centre (bottom) for t0..t1, with subtitle lines marked."""
    from PIL import Image, ImageDraw
    d, ch = load(path)
    subs = srt_for(path)
    W, H = 1800, 360
    sheet = Image.new("RGB", (W, 2 * H + 70), (10, 10, 10))
    dr = ImageDraw.Draw(sheet)
    for k, name in enumerate(("nc", "ce")):
        seg = d[name][int(t0 * SR):int(t1 * SR)]
        sheet.paste(spec_image(seg, W, H), (0, 20 + k * (H + 20)))
        dr.text((4, 4 + k * (H + 20)), "%s %s t=%.1f..%.1f" % (name, tag(path), t0, t1), fill=(255, 255, 0))
    for sec in range(int(np.ceil(t0)), int(t1) + 1):
        x = int((sec - t0) / (t1 - t0) * W)
        dr.line([(x, 20), (x, 28)], fill=(255, 255, 255))
        if sec % 2 == 0:
            dr.text((x + 2, 20), str(sec), fill=(255, 255, 255))
    y = 2 * H + 44
    for s0, s1, tx in subs:
        if s1 > t0 and s0 < t1:
            x0, x1 = int((max(s0, t0) - t0) / (t1 - t0) * W), int((min(s1, t1) - t0) / (t1 - t0) * W)
            dr.rectangle([x0, 2 * H + 40, x1, 2 * H + 46], fill=(255, 120, 0))
            dr.text((x0, y), tx[:40], fill=(255, 180, 100))
            y += 10 if y < 2 * H + 62 else -20
    sheet.save(out)


def cmd_spec(a):
    big_spec(match([a.patterns[0]])[0], a.t0, a.t1, a.out)


if __name__ == "__main__":
    main()


def timelapse(path, t0, t1, step, out, cols=6, w=200):
    """Contact sheet of one frame every `step` seconds (one ffmpeg call: fps filter + tile)."""
    r = run([FFMPEG, "-v", "error", "-ss", "%.2f" % t0, "-t", "%.2f" % (t1 - t0), "-i", path, "-vf",
             "fps=1/%.3f,scale=%d:-2,tile=%dx%d" % (
                 step, w, cols, max(1, int(np.ceil((t1 - t0) / step / cols)))), "-frames:v", "1", "-y", out])
    return r.returncode == 0


def stft(x, nfft=1024, hop=256):
    w = np.hanning(nfft).astype(np.float32)
    n = 1 + max(0, -(-(len(x) - nfft) // hop))
    pad = np.zeros((n - 1) * hop + nfft, np.float32)
    pad[:len(x)] = x
    idx = np.arange(n)[:, None] * hop + np.arange(nfft)[None, :]
    return np.fft.rfft(pad[idx] * w, axis=1), w


def istft(S, w, hop=256, length=None):
    nfft = (S.shape[1] - 1) * 2
    fr = np.fft.irfft(S, axis=1) * w
    out = np.zeros((S.shape[0] - 1) * hop + nfft, np.float32)
    nrm = np.zeros_like(out)
    for i in range(S.shape[0]):
        out[i * hop:i * hop + nfft] += fr[i]
        nrm[i * hop:i * hop + nfft] += w * w
    out /= np.maximum(nrm, 1e-6)
    return out[:length] if length else out


def bed_reduce(x, sr, pre_n, alpha=1.6, beta=0.12):
    """Spectral subtraction of a steady music bed. x = pre_n samples of bed (before the sound) + the sound.
    The bed profile is the per-bin 80th percentile of the pre-roll magnitude; returns only the part after pre_n."""
    S, w = stft(x)
    mag, ph = np.abs(S), np.angle(S)
    pre_frames = max(3, pre_n // 256 - 3)
    prof = np.percentile(mag[:pre_frames], 80, axis=0)
    prof = np.convolve(prof, np.ones(5) / 5, mode="same")
    clean = np.maximum(mag - alpha * prof[None, :], beta * mag)
    y = istft(clean * np.exp(1j * ph), w, length=len(x))
    return y[pre_n:]


def video_stats(path, t0, t1, fps=12, w=96, h=54):
    """Per-frame stats of a window, decoded small: (times, luma, warm, green, violet, white). Counts are pixel fractions.
    warm = orange/red/yellow beam or fire, green = disruptor/Borg green, violet = polaron/blue-violet, white = flash."""
    r = run([FFMPEG, "-v", "error", "-hwaccel", "cuda", "-hwaccel_output_format", "cuda", "-ss", "%.2f" % t0, "-t", "%.2f" % (t1 - t0),
             "-i", path, "-vf", "fps=%d,scale_cuda=%d:%d,hwdownload,format=p010le,scale=%d:%d,format=rgb24" % (fps, w, h, w, h),
             "-f", "rawvideo", "-"])
    if len(r.stdout) < w * h * 3:   # no CUDA decode for this file: software
        r = run([FFMPEG, "-v", "error", "-ss", "%.2f" % t0, "-t", "%.2f" % (t1 - t0), "-i", path, "-vf", "fps=%d,scale=%d:%d" % (fps, w, h),
                 "-pix_fmt", "rgb24", "-f", "rawvideo", "-"])
    a = np.frombuffer(r.stdout, np.uint8)
    n = len(a) // (w * h * 3)
    if n == 0:
        return None
    f = a[:n * w * h * 3].reshape(n, h * w, 3).astype(np.int16)
    R, G, B = f[..., 0], f[..., 1], f[..., 2]
    luma = (0.3 * R + 0.59 * G + 0.11 * B).mean(axis=1)
    warm = ((R > 170) & (R > B + 70) & (G > 40)).mean(axis=1)
    green = ((G > 150) & (G > R + 50) & (G > B + 30)).mean(axis=1)
    violet = ((B > 150) & (B > G + 40) & (R > 60)).mean(axis=1)
    white = ((R > 225) & (G > 225) & (B > 200)).mean(axis=1)
    return t0 + np.arange(n) / fps, luma, warm, green, violet, white


def vscan(path, fps=8, w=64, h=36):
    """Whole-episode low-res video stats, cached: arrays t, luma, warm, green, violet, white (see video_stats)."""
    cp = os.path.join(CACHE, tag(path) + "_vid.npz")
    if os.path.exists(cp):
        z = np.load(cp)
        return tuple(z[k] for k in ("t", "luma", "warm", "green", "violet", "white"))
    dur = float(probe(path).get("format", {}).get("duration", 0) or 0)
    out = video_stats(path, 0, dur + 1, fps, w, h)
    if out is None:
        return None
    np.savez(cp, **dict(zip(("t", "luma", "warm", "green", "violet", "white"), out)))
    return out


def strips(items, out, w=1500, h=130, fmax=9000):
    """Stacked spectrogram strips. items = [(pattern, t0, t1, label)]; every 0.5 s a tick, every second a number."""
    from PIL import Image, ImageDraw
    sheet = Image.new("RGB", (w, len(items) * (h + 18)), (10, 10, 10))
    dr = ImageDraw.Draw(sheet)
    for k, (pat, t0, t1, label) in enumerate(items):
        f = match([pat])[0]
        d, ch = load(f)
        seg = d["nc"][int(t0 * SR):int(t1 * SR)]
        y = k * (h + 18)
        sheet.paste(spec_image(seg, w, h), (0, y + 18))
        dr.text((3, y + 3), "%d %s %s %.2f..%.2f" % (k, label, tag(f), t0, t1), fill=(255, 255, 0))
        t = np.ceil(t0 * 2) / 2
        while t < t1:
            x = int((t - t0) / (t1 - t0) * w)
            dr.line([(x, y + 18), (x, y + 24 if t % 1 else y + 30)], fill=(255, 255, 255))
            if t % 1 == 0:
                dr.text((x + 2, y + 18), "%g" % t, fill=(255, 255, 255))
            t += 0.5
    sheet.save(out)


def loudness_match(x, rms_db=-20.0, peak_lim_db=-1.5):
    """Gain so the active part (frames within 35 dB of the peak) sits at rms_db, never past peak_lim_db."""
    k = 320
    n = len(x) // k
    fr = np.sqrt(np.mean(x[:n * k].reshape(n, k) ** 2, axis=1)) if n else np.array([np.max(np.abs(x))])
    act = fr[fr > fr.max() * 10 ** (-35 / 20)]
    rms = float(np.sqrt(np.mean(act ** 2))) if len(act) else float(fr.max())
    g = 10 ** (rms_db / 20) / max(rms, 1e-9)
    pk = float(np.max(np.abs(x))) * g
    if pk > 10 ** (peak_lim_db / 20):
        g *= 10 ** (peak_lim_db / 20) / pk
    return x * g


def mmss(t):
    return "%d:%04.1f" % (int(t // 60), t % 60)


def cmd_build(a):
    """picks.json -> wavs in spec.out, merged trek cues.json, copies and listen-sheet.html in spec.sheet_dir."""
    import shutil
    spec = json.load(open(a.patterns[0], encoding="utf-8"))
    out, sheet_dir = spec["out"], spec["sheet_dir"]
    os.makedirs(sheet_dir + "/clips", exist_ok=True)
    rows, cues = [], {}
    for c in spec["clips"]:
        f = match([c["src"]])[0]
        ch = channels(f)
        pre = float(c.get("pre", 0.0))
        x = clip_samples(f, c["t"] - pre, c["dur"] + pre, ch, c.get("mode", "nc"))
        y = shape_clip(x, OUT_SR, max_len=c.get("max_len"), fade_out=c.get("fade_out", 0.05))
        if y is None:
            print("EMPTY", c["file"])
            continue
        y = loudness_match(y, c.get("rms_db", -20.0))
        write_wav(os.path.join(out, c["file"]), y)
        shutil.copy2(os.path.join(out, c["file"]), os.path.join(sheet_dir, "clips", c["file"]))
        for k in c["keys"]:
            cues.setdefault(k, []).append("fx/" + c["file"])
        rows.append({"clip": c, "len": len(y) / OUT_SR, "episode": os.path.basename(f), "time": mmss(c["t"])})
        print("built", c["file"], "%.2fs" % (len(y) / OUT_SR))
    json.dump({"rows": rows, "cues": cues}, open(os.path.join(sheet_dir, "build.json"), "w"), indent=1)


def cmd_html(a):
    """listen-sheet.html from build.json: coverage per key (clips with <audio>, source episode, time) and a clip table."""
    import html
    spec = json.load(open(a.patterns[0], encoding="utf-8"))
    sheet_dir = spec["sheet_dir"]
    b = json.load(open(os.path.join(sheet_dir, "build.json")))
    cues, rows = b["cues"], b["rows"]
    by_file = {r["clip"]["file"]: r for r in rows}
    esc = html.escape
    out = ["<!doctype html><meta charset=utf-8><title>BOTF M4-AUDIO listen sheet</title>",
           "<style>body{font:14px Segoe UI,sans-serif;background:#111;color:#ddd;margin:18px}table{border-collapse:collapse;width:100%}"
           "td,th{border-bottom:1px solid #333;padding:4px 8px;vertical-align:top}th{text-align:left;color:#fc0}.key{color:#8cf;font-family:Consolas,monospace}"
           ".bed{color:#f84}.ok{color:#6d6}.none{color:#888}h2{color:#fc0;margin-top:26px}audio{height:28px}.n{color:#999;font-size:12px}"
           "button{margin:2px}.mark{font-size:12px}</style>",
           "<h1>BOTF Remastered: weapon, hit and explosion sounds (M4-AUDIO, 14 Nov 2026)</h1>",
           "<p>Private; clips are cut from your own episodes and films and never go in git. Nothing here was checked by ear. "
           "Play each, mark <b>ok</b> or <b>no</b> (kept in this browser), then press <i>copy marks</i> and paste them back. "
           "<b class=bed>bed</b> = a battle or music bed under the sound (measured: the sound peaks less than 12 dB over the quietest 10% of the 4.5 s around it); "
           "<b class=ok>clean</b> = 20 dB or more.</p>",
           "<p><button onclick=copyMarks()>copy marks</button> <span id=cnt class=n></span></p>"]
    groups = spec.get("groups", [])
    allk = spec.get("all_keys", sorted(cues))
    done = set()
    for g in groups:
        out.append("<h2>%s</h2><table><tr><th>key</th><th>clips</th></tr>" % esc(g["title"]))
        for k in g["keys"]:
            done.add(k)
            files = cues.get(k, [])
            cells = []
            for f in files:
                fn = f.split("/")[-1]
                r = by_file.get(fn)
                c = r["clip"] if r else {}
                tier = c.get("tier", "")
                cells.append("<div><audio controls preload=none src=\"clips/%s\"></audio> <span class=mark>"
                             "<label><input type=radio name=\"%s|%s\" value=ok onchange=mark(this)>ok</label> "
                             "<label><input type=radio name=\"%s|%s\" value=no onchange=mark(this)>no</label></span> "
                             "<span class=n>%s | %s @ %s | %.1fs | <span class=%s>%s</span> | %s</span></div>" % (
                                 esc(fn), esc(k), esc(fn), esc(k), esc(fn), esc(fn),
                                 esc(r["episode"] if r else "?"), r["time"] if r else "?", r["len"] if r else 0,
                                 "bed" if tier == "bed" else "ok", "bed" if tier == "bed" else "clean" if tier == "clean" else tier or "-",
                                 esc(c.get("note", ""))))
            out.append("<tr><td class=key>%s</td><td>%s</td></tr>" % (esc(k), "".join(cells) or "<span class=none>tone fallback (no clip)</span>"))
        out.append("</table>")
    rest = [k for k in allk if k not in done]
    if rest:
        out.append("<h2>Other keys</h2><table>")
        for k in rest:
            out.append("<tr><td class=key>%s</td><td>%s</td></tr>" % (esc(k), ", ".join(esc(f.split("/")[-1]) for f in cues.get(k, [])) or "<span class=none>tone fallback</span>"))
        out.append("</table>")
    out.append("<script>const K='botf-m4-audio-marks';let M=JSON.parse(localStorage.getItem(K)||'{}');"
               "function mark(e){M[e.name]=e.value;localStorage.setItem(K,JSON.stringify(M));cnt()}"
               "function cnt(){document.getElementById('cnt').textContent=Object.keys(M).length+' marked'}"
               "document.querySelectorAll('input[type=radio]').forEach(e=>{if(M[e.name]===e.value)e.checked=true});cnt();"
               "function copyMarks(){navigator.clipboard.writeText(Object.entries(M).map(([k,v])=>v+'  '+k).join(String.fromCharCode(10)))}</script>")
    open(os.path.join(sheet_dir, "listen-sheet.html"), "w", encoding="utf-8").write(chr(10).join(out))
    print("wrote", os.path.join(sheet_dir, "listen-sheet.html"))


def main():
    ap = argparse.ArgumentParser()
    sp = ap.add_subparsers(dest="cmd", required=True)
    for name in ("find", "events", "sheet", "cut", "spec", "build", "html"):
        p = sp.add_parser(name)
        p.add_argument("patterns", nargs="*")
        p.add_argument("--out")
        p.add_argument("--t0", type=float)
        p.add_argument("--t1", type=float)
    a = ap.parse_args()
    globals()["cmd_" + a.cmd](a)


if __name__ == "__main__":
    main()
