#!/usr/bin/env python3
"""M4-AUDIO: builds the weapon, hit, explosion, warp and cloak sounds from Simon's own Star Trek: Bridge Commander installs
(game SFX: clean one-shots, per race and era). Private, read only, never committed, no downloads; the runtime never reads H:.

  python build_trek_fx.py --out <LocalAssets/audio/trek> [--sheet <dir>] [--only weapon.Disruptor]

Writes <out>/fx/<id>_<n>.wav (32 kHz mono 16-bit, 40 Hz high-pass, tail trimmed, faded, loudness matched per class) and merges
its keys into <out>/cues.json (every earlier weapon.*, sfx.shield.*, sfx.explosion.*, sfx.warp.*, sfx.cloak.*, sfx.decloak.*
key is replaced; voice and alert keys are kept). With --sheet it also writes build.json for the listen sheet
(mine_trek_weapons.py html). Key names are the ones battle_timeline.gd and botf_audio_cues.gd request:
weapon.<Type>.<fac>.<era>, weapon.<Type>.<fac>, weapon.<Type>.<era>, weapon.<Type>, weapon.generic.<beam|torpedo>(.<era>).
"""
import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import wave

import numpy as np

FFMPEG = os.environ.get("FFMPEG", "ffmpeg")
SR = 32000
KM = r"H:\Games\Star Trek Bridge Commander KM"
ROOTS = [KM + r"\sfx\Weapons", KM + r"\sfx", KM + r"\scripts\Custom\NanoFXv2\ExplosionFX\Sfx", r"H:\Games\Star Trek Bridge Commander base\sfx",
         r"H:\StarhelmTools\bc-src"]
RMS_DB = {"beam": -20.0, "pulse": -21.0, "torp": -19.0, "shield": -20.0, "hull": -19.0, "expl": -17.0, "warp": -20.0, "cloak": -22.0}
MAXLEN = {"beam": 2.2, "pulse": 1.4, "torp": 2.4, "shield": 1.6, "hull": 1.3, "expl": 3.2, "warp": 3.0, "cloak": 2.6}

# (id, class, [keys], [source stems], note). A source stem is a file name without .wav, found under ROOTS (first hit wins).
FED, KLI, ROM, CAR, DOM, BREEN, BORG, FER = "fed", "kli", "rom", "car", "dom", "breen", "borg", "fer"


def k(types, fac, era=""):
    out = []
    for t in types:
        out.append("weapon.%s.%s%s" % (t, fac, "." + era if era else ""))
    return out


BEAMS_FED = ["PhaserBank", "PhaserArray"]
SETS = []


def add(i, cls, keys, srcs, note=""):
    SETS.append({"id": i, "cls": cls, "keys": keys, "srcs": srcs, "note": note})


# --- Federation phasers, by era (generic.beam twin carries the era for any race without its own clip)
add("fed_phaser_tos", "beam", k(BEAMS_FED, FED, "tos") + ["weapon.generic.beam.tos"], ["F_TOS_PHASER_START", "F_TMP_PHASER_START", "TmpPhaser"], "TOS and TMP-era phaser")
add("fed_phaser_ent", "beam", k(BEAMS_FED, FED, "ent") + ["weapon.generic.beam.ent"], ["F_ENT_PHASER_BLAST", "F_ENT_PHASER_LOOP"], "ENT phase cannon sounds")
add("fed_phaser_tng", "beam", k(BEAMS_FED, FED, "tng") + ["weapon.generic.beam.tng"],
    ["F_TNG_PHASER_BLAST_1", "F_TNG_PHASER_BLAST_2", "F_TNG_PHASER_BLAST_3", "galaxy_phaser_a", "ambassador_phaser_a"], "TNG and movie arrays")
add("fed_phaser_ds9", "beam", k(BEAMS_FED, FED, "ds9") + k(BEAMS_FED, FED) + ["weapon.generic.beam.ds9", "weapon.generic.beam", "weapon.PhaserBank", "weapon.PhaserArray"],
    ["KO_DS9Phaser", "GalaxyDS9_phaser1", "GalaxyDS9_phaser2", "ds9phaser_a"], "DS9 arrays; also the era-less default")
add("fed_phaser_voy", "beam", k(BEAMS_FED, FED, "voy") + ["weapon.generic.beam.voy"], ["VoyPhaserAJLH", "VoyPhaserBJLH", "KO_FedFutureA"], "Voyager arrays")
# Pulse phasers and phase cannons (Defiant, Enterprise NX-01, Ferengi)
add("fed_pulse", "pulse", k(["PulsePhaser", "PhaseCannon"], FED) + k(["PulsePhaser", "PhaseCannon"], FED, "ds9") + k(["PulsePhaser", "PhaseCannon"], FED, "tng")
    + k(["PulsePhaser", "PhaseCannon"], FED, "voy"), ["F_TNG_PULSE_PHASER", "PulsePhaserJLH", "PPhaser"], "Defiant pulse phaser")
add("fed_cannon_ent", "pulse", k(["PulsePhaser", "PhaseCannon"], FED, "ent") + k(["PulsePhaser", "PhaseCannon"], FED, "tos"), ["F_ENT_PHASER_BLAST", "F_TOS_PHASER_START"], "ENT phase cannon")
add("fer_phaser", "beam", k(["PhaserArray", "PhaseCannon", "PhaserBank"], FER), ["ferengi_phaser_a", "ferengi_phaser_b", "med_phaser_a"], "Ferengi")
# --- Federation torpedoes
PT = ["PhotonTorpedo", "PhotonicTorpedo"]
add("fed_photon_tos", "torp", k(PT, FED, "tos") + ["weapon.generic.torpedo.tos"], ["F_TOS_PHOTON", "TmpPhoton", "FedTmpTorpedo"], "TOS and TMP photon")
add("fed_photon_ent", "torp", k(PT, FED, "ent") + ["weapon.generic.torpedo.ent", "weapon.SpatialTorpedo.ent", "weapon.SpatialTorpedo"], ["F_ENT_PHOTON", "FedPulsePhotonTorp"], "ENT photonic and spatial")
add("fed_photon_tng", "torp", k(PT, FED, "tng") + ["weapon.generic.torpedo.tng"], ["F_TNG_PHOTON_1", "F_TNG_PHOTON_5", "F_TNG_PHOTON_6", "FedPhotonTorp"], "TNG photon")
add("fed_photon_ds9", "torp", k(PT, FED, "ds9") + k(PT, FED) + ["weapon.generic.torpedo.ds9", "weapon.generic.torpedo", "weapon.Torpedo", "weapon.PhotonTorpedo"],
    ["KO_FedPhoton", "FTB Photon Torp", "FTBPhotonTorp2", "Photon01Nano"], "DS9 photon; also the era-less default")
add("fed_photon_voy", "torp", k(PT, FED, "voy") + ["weapon.generic.torpedo.voy"], ["VoyTorpJLH", "Photon01JLH", "Micro Photon Torp"], "Voyager photon")
add("fed_quantum", "torp", ["weapon.QuantumTorpedo.fed", "weapon.QuantumTorpedo.fed.ds9", "weapon.QuantumTorpedo.fed.voy", "weapon.QuantumTorpedo.fed.tng",
                              "weapon.QuantumTorpedo"], ["Quantum Torp", "F_TNG_QUANTUM_DEFIANT", "DGQuantumK", "FLX_TypeX_Quantum_Torp"], "quantum")
# --- Klingon
add("kli_beam", "beam", k(["Disruptor"], KLI) + k(["Disruptor"], KLI, "ds9") + k(["Disruptor"], KLI, "voy") + ["weapon.Disruptor"],
    ["KlingonDisruptorBeam_a", "KlingonDisruptorBeam_b", "klingon_beam_a", "klingon_beam_b"], "Klingon disruptor beam")
add("kli_beam_tng", "beam", k(["Disruptor"], KLI, "tng"), ["K_TNG_PHASER_START", "K_TNG_PHASER_LOOP", "klingon_beam_a"], "TNG Klingon beam")
add("kli_beam_tos", "pulse", k(["Disruptor", "DisruptorCannon"], KLI, "tos") + k(["Disruptor", "DisruptorCannon"], KLI, "ent"), ["K_TOS_TMP_PULSE", "d7tosdcannon2"], "TOS and TMP Klingon")
add("kli_cannon", "pulse", k(["DisruptorCannon"], KLI) + k(["DisruptorCannon"], KLI, "ds9") + k(["DisruptorCannon"], KLI, "voy") + ["weapon.DisruptorCannon"],
    ["Klingon Cannon", "Disruptor Cannon", "Klingon Pulse Disruptor", "KO_Ktinga2"], "Klingon cannon bolts")
add("kli_cannon_tng", "pulse", k(["DisruptorCannon"], KLI, "tng"), ["K_TNG_PULSE", "Klingon Cannon"], "TNG Klingon pulse")
add("kli_torp", "torp", k(["Torpedo", "PhotonTorpedo"], KLI) + k(["Torpedo", "PhotonTorpedo"], KLI, "ds9") + k(["Torpedo", "PhotonTorpedo"], KLI, "tng")
    + k(["Torpedo", "PhotonTorpedo"], KLI, "voy"), ["Klingon Torp", "Klingon Photon Torp", "KO_Klingon RedTorpedo", "KO_Klingon GreenTorpedo"], "Klingon torpedo")
add("kli_torp_tos", "torp", k(["Torpedo", "PhotonTorpedo"], KLI, "tos") + k(["Torpedo", "PhotonTorpedo"], KLI, "ent"), ["K_TOS_TMP_PHOTON", "KlingTmpTorpedo"], "TOS and TMP Klingon torpedo")
# --- Romulan
add("rom_beam", "beam", k(["Disruptor"], ROM) + k(["Disruptor"], ROM, "ds9") + k(["Disruptor"], ROM, "voy") + k(["Disruptor"], ROM, "tng"),
    ["romulan phaser_a", "romulan phaser_b", "RemanDisruptor", "Scimitar_Disruptor", "R_TNG_PULSE"], "Romulan disruptor")
add("rom_beam_tos", "beam", k(["Disruptor"], ROM, "tos") + k(["Disruptor"], ROM, "ent"), ["R_TOS_PHASER_START", "R_TOS_PHASER_LOOP"], "TOS Romulan")
add("rom_torp", "torp", k(["PlasmaTorpedo", "Torpedo", "SpatialTorpedo"], ROM) + k(["PlasmaTorpedo", "Torpedo", "SpatialTorpedo"], ROM, "ds9")
    + k(["PlasmaTorpedo", "Torpedo", "SpatialTorpedo"], ROM, "tng") + k(["PlasmaTorpedo", "Torpedo", "SpatialTorpedo"], ROM, "voy") + ["weapon.PlasmaTorpedo"],
    ["Plasma Torpedo", "RomTorpJLH", "R_TNG_PLASMA", "Scimitar_Torpedo"], "Romulan plasma torpedo")
add("rom_torp_tos", "torp", k(["PlasmaTorpedo", "Torpedo", "SpatialTorpedo"], ROM, "tos") + k(["PlasmaTorpedo", "Torpedo", "SpatialTorpedo"], ROM, "ent"), ["R_TOS_PHOTON", "Plasma Torpedo"], "TOS Romulan plasma")
# --- Cardassian
add("car_beam", "beam", k(["Disruptor"], CAR) + k(["Disruptor"], CAR, "ds9") + k(["Disruptor"], CAR, "tng") + k(["Disruptor"], CAR, "voy"),
    ["cardassian_beam_a", "cardassian_beam_b", "KO_CardassianA", "KO_CardassianB", "CADS9Phaser"], "Cardassian beam")
add("car_cannon", "pulse", k(["DisruptorCannon"], CAR), ["Cardassian Cannon", "cardassian_phaser_a"], "Cardassian cannon")
add("car_torp", "torp", k(["PhotonTorpedo", "Torpedo"], CAR) + k(["PhotonTorpedo", "Torpedo"], CAR, "ds9") + k(["PhotonTorpedo", "Torpedo"], CAR, "tng"),
    ["Cardassian Torp", "CADS9Torp", "CAJunk_Torp"], "Cardassian torpedo")
add("car_tricobalt", "torp", k(["TricobaltDevice"], CAR) + ["weapon.TricobaltDevice"], ["TricobaltTorpedo"], "tricobalt device")
# --- Dominion, Breen, Borg
add("dom_polaron", "beam", k(["PolaronBeam"], DOM) + ["weapon.PolaronBeam"], ["FTBPolaronBeam_a", "FTBPolaronBeam_b", "KO_PoleronB", "D_POLARON_A_START"], "Jem'Hadar polaron")
add("dom_torp", "torp", k(["Torpedo"], DOM), ["D_TNG_POLARON_TORP", "Cardassian Torp"], "Dominion torpedo")
add("breen_dampener", "beam", k(["EnergyDampener", "Disruptor"], BREEN), ["Breen1", "Breen2"], "Breen energy dampener")
add("breen_torp", "torp", k(["Torpedo"], BREEN), ["breen_torpedo"], "Breen torpedo")
add("borg_cutter", "beam", k(["CuttingBeam"], BORG) + ["weapon.CuttingBeam"], ["borg_beam_a", "borg_beam_b", "KO_BorgCutter", "cutJLH", "B_TNG_CUTTER"], "Borg cutting beam")
add("borg_torp", "torp", k(["Torpedo"], BORG), ["BorgTorpJLH", "DYborgtorpedo", "B_TNG_TORP", "DGBorgTorp"], "Borg torpedo")
# --- generic fallbacks for minor races (no faction key)
add("gen_disruptor", "beam", ["weapon.Disruptor.ds9", "weapon.Disruptor.tng"], ["Disruptor", "Nierrh Disruptor", "Pulse Disruptor"], "generic disruptor")
add("gen_plasma", "pulse", ["weapon.PlasmaCannon"], ["Plasma Bolt", "Plasma Pulse"], "plasma cannon (Orion, Xindi, Vulcan...)")
add("gen_bio", "beam", ["weapon.BioEnergy"], ["CA8472Beam"], "Species 8472")
# --- hits, explosions, warp, cloak
add("shield_hit", "shield", ["sfx.shield.hit"], ["shield 11", "shield 12", "shield 14", "shield 10", "HitShield2"], "shield hit")
add("hull_hit", "hull", ["sfx.explosion.small"], ["small 27", "small 28", "small 29", "small 30", "small 47"], "hull hit, small explosion")
add("explosion_large", "expl", ["sfx.explosion.large"], ["explo_large_01", "explo_large_03", "explo_large_05", "explo_large_07"], "ship destroyed")
add("warp_out", "warp", ["sfx.warp.out"], ["enter warp", "warp flash"], "warp out (retreat)")
add("warp_in", "warp", ["sfx.warp.in"], ["exit warp"], "warp in (fleet arrives)")
add("cloak", "cloak", ["sfx.cloak.rom", "sfx.cloak.kli", "sfx.cloak.fed", "sfx.cloak.car"], ["cloak on", "R_CLOAK_ON"], "cloak")
add("decloak", "cloak", ["sfx.decloak.rom", "sfx.decloak.kli", "sfx.decloak.fed", "sfx.decloak.car"], ["cloak off", "R_CLOAK_OFF"], "decloak")

REPLACED = ("weapon.", "sfx.shield.", "sfx.explosion.", "sfx.warp.", "sfx.cloak.", "sfx.decloak.")


def build_index():
    idx = {}
    for root in ROOTS:
        for dp, dn, fn in os.walk(root):
            if root.endswith("bc-src") and not re.search(r"weapons|nanofx|explosion", dp, re.I):
                continue
            for f in fn:
                if f.lower().endswith(".wav"):
                    idx.setdefault(f[:-4].lower(), os.path.join(dp, f))
    return idx


def decode(path):
    r = subprocess.run([FFMPEG, "-v", "error", "-i", path, "-ac", "1", "-ar", str(SR), "-f", "f32le", "-"], capture_output=True)
    return np.frombuffer(r.stdout, np.float32).copy()


def shape(x, cls):
    if len(x) < SR * 0.05:
        return None
    X = np.fft.rfft(x)
    X[np.fft.rfftfreq(len(x), 1 / SR) < 40] = 0
    x = np.fft.irfft(X, len(x))
    pk = float(np.max(np.abs(x)))
    if pk < 1e-5:
        return None
    k = int(0.01 * SR)
    env = np.convolve(np.abs(x), np.ones(k) / k, mode="same")
    live = np.where(env > pk * 10 ** (-48 / 20))[0]
    a, b = max(0, live[0] - int(0.003 * SR)), min(len(x), live[-1] + int(0.02 * SR))
    x = x[a:b]
    cap = MAXLEN[cls]
    fo = 0.06
    if len(x) > cap * SR:
        x = x[:int(cap * SR)]
        fo = 0.25
    fi, fo_n = min(int(0.003 * SR), len(x) // 2), min(int(fo * SR), len(x) // 2)
    x[:fi] *= np.linspace(0, 1, fi)
    x[-fo_n:] *= np.linspace(1, 0, fo_n)
    fr = 320
    n = len(x) // fr
    rms_fr = np.sqrt(np.mean(x[:n * fr].reshape(n, fr) ** 2, axis=1)) if n else np.array([np.max(np.abs(x))])
    act = rms_fr[rms_fr > rms_fr.max() * 10 ** (-35 / 20)]
    rms = float(np.sqrt(np.mean(act ** 2))) if len(act) else float(rms_fr.max())
    g = 10 ** (RMS_DB[cls] / 20) / max(rms, 1e-9)
    if np.max(np.abs(x)) * g > 10 ** (-1.5 / 20):
        g = 10 ** (-1.5 / 20) / np.max(np.abs(x))
    return x * g


def write_wav(path, x):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes((np.clip(x, -1, 1) * 32767).astype("<i2").tobytes())


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--out", required=True)
    ap.add_argument("--sheet")
    ap.add_argument("--only")
    a = ap.parse_args()
    idx = build_index()
    fxdir = os.path.join(a.out, "fx")
    os.makedirs(fxdir, exist_ok=True)
    cues, rows, missing = {}, [], []
    for s in SETS:
        seen_hash = set()
        n = 0
        for stem in s["srcs"]:
            p = idx.get(stem.lower())
            if not p:
                missing.append(stem)
                continue
            h = hashlib.md5(open(p, "rb").read()).hexdigest()
            if h in seen_hash:
                continue
            seen_hash.add(h)
            y = shape(decode(p), s["cls"])
            if y is None:
                missing.append(stem + " (empty)")
                continue
            n += 1
            fn = "%s_%d.wav" % (s["id"], n)
            write_wav(os.path.join(fxdir, fn), y)
            for key in s["keys"]:
                cues.setdefault(key, []).append("fx/" + fn)
            rows.append({"clip": {"file": fn, "keys": s["keys"], "note": s["note"], "tier": "clean", "cls": s["cls"]},
                         "len": len(y) / SR, "episode": "Bridge Commander SFX: " + os.path.relpath(p, "H:\\").replace("\\", "/"), "time": "one-shot"})
    cp = os.path.join(a.out, "cues.json")
    old = json.load(open(cp, encoding="utf-8")) if os.path.exists(cp) else {"cues": {}}
    kept = {key: v for key, v in old["cues"].items() if not key.startswith(REPLACED)}
    kept.update(cues)
    old["cues"] = kept
    old["note"] = "private; weapon, hit, explosion, warp and cloak keys from build_trek_fx.py (Bridge Commander SFX); the rest from mine_trek_audio.py"
    json.dump(old, open(cp, "w", encoding="utf-8"), indent=1)
    print("%d clips, %d keys; missing sources: %s" % (len(rows), len(cues), missing))
    if a.sheet:
        os.makedirs(os.path.join(a.sheet, "clips"), exist_ok=True)
        for r in rows:
            shutil.copy2(os.path.join(fxdir, r["clip"]["file"]), os.path.join(a.sheet, "clips", r["clip"]["file"]))
        json.dump({"rows": rows, "cues": cues}, open(os.path.join(a.sheet, "build.json"), "w"), indent=1)


if __name__ == "__main__":
    main()
