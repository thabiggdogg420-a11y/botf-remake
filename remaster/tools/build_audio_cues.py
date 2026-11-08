#!/usr/bin/env python3
"""Builds LocalAssets/audio/cues.json for the Godot client: sound key -> local files, from the audio extracted from
the user's own Birth of the Federation (see the rebuild steps below). Private and local only; the output is gitignored.

Rebuild from nothing (all no-login, archive.org items `birthof-fed` and `botf_20240331`, see docs/AUDIO-SOURCES-ONLINE.md):
  1. download "Birthof Fed.zip" (birthof-fed) and BOTF.iso (botf_20240331) into fresh folders;
  2. unzip english.snd, sfx.snd, CD/music.snd and stbof.res into one folder (the install);
  3. python game/Tools/Extract-BotfAudio.py --botf <install> --out game/LocalAssets/botf-original/audio
  4. convert: voice and music to ogg (libvorbis), sfx to 22.05 kHz wav, into game/LocalAssets/audio/{voice,music,sfx}
     (tools/convert_botf_audio.py does exactly this with ffmpeg);
  5. 7z x BOTF.iso "ENGLISH\\*EOG*.wav" -> ogg into game/LocalAssets/audio/eog (the end-of-game speeches);
  6. python tools/botf_voice_groups.py --res <install>/stbof.res --out game/LocalAssets/audio/voice/voice-groups.json
  7. python tools/build_audio_cues.py --audio game/LocalAssets/audio

What maps where:
  voice  - converse.bin gives every english.snd clip its race (0 Cardassian, 1 Federation, 2 Ferengi, 3 Klingon,
           4 Romulan) and a conversation group (one situation per group). VOICE_GROUPS names the group for each key;
           the group's text was read from a Whisper transcript. Keys are vo.<...>.<fac> with fac fed/kli/rom/car/fer.
  music  - music.snd tracks: 0 title; 1-5 strategic music per race; 6-10 combat music per race; 11-15 unlabelled.
           Race order follows the race index above. The quieter half of a race's combat segments is the approach
           (mus.botf.combat.<fac>); the louder half is the fight (mus.botf.combat.tension.<fac>).
  sfx    - sfx.snd has no names. SFX below is a HEURISTIC pick by length, spectrum and envelope (long low decays are
           explosions, steady 1.5 s tones are beams, short blips are the interface). Judge by ear and edit SFX.
  eog    - ENGLISH/?EOG{Alli,Anni,Domi,Loss}.wav: the end-of-game speeches (C Cardassian, F Ferengi, H Federation,
           K Klingon, R Romulan): vo.victory.{diplomatic,conquest,domination,win,defeat}.<fac>.
"""
import argparse
import json
import os
import re
import subprocess
import shutil
import sys

RACE_FAC = {0: "car", 1: "fed", 2: "fer", 3: "kli", 4: "rom"}
EOG_FAC = {"C": "car", "F": "fer", "H": "fed", "K": "kli", "R": "rom"}

# key template -> converse.bin groups (the Federation member's wording, for reference)
VOICE_GROUPS = {
    "vo.combat.ack.{fac}": [54, 55, 56, 57, 58, 59],            # "Yes, sir." "Understood." "Acknowledged."
    "vo.combat.kill.{fac}": [64],                                # "Target destroyed."
    "vo.combat.torpedo.{fac}": [73],                             # "Torpedoes firing!"
    "vo.combat.phaser.{fac}": [72],                              # "Fire phasers!"
    "vo.combat.shields.75.{fac}": [69],                          # "Shields are holding."
    "vo.combat.shields.50.{fac}": [70],                          # "Shields compromised. Evasive maneuvers."
    "vo.combat.shields.25.{fac}": [70],
    "vo.combat.shields.{fac}": [69, 70],
    "vo.combat.cloak.{fac}": [60],                               # "Cloaking device engaged."
    "vo.combat.decloak.{fac}": [61],                             # "Disengage cloak."
    "vo.combat.retreat.{fac}": [67],                             # "Retreat course engaged."
    "vo.combat.evade.{fac}": [80],                               # "Evasive action."
    "vo.combat.hail.{fac}": [78],                                # "Open a channel."
    "vo.combat.hailed.{fac}": [77],                              # "We're being hailed."
    "vo.combat.encounter.{fac}": [53],                           # "Select a target."
    "vo.combat.underattack.{fac}": [66],                         # "We're under attack!"
    "vo.combat.enemyretreat.{fac}": [65],                        # "The enemy is in retreat."
    "vo.combat.won.{fac}": [50],                                 # "The battle is over. We've won."
    "vo.combat.lost.{fac}": [45],                                # "Our forces have been defeated."
    "vo.combat.peace.{fac}": [43, 44],                           # "Another peaceful encounter."
    "vo.combat.disengaged.{fac}": [46, 47, 49],                  # "We've disengaged for now."
    "vo.advisor.attack.{fac}": [66],                             # "We're under attack!" (system under attack)
    "vo.advisor.contact.{fac}": [8],                             # "An alien culture has been discovered."
    "vo.advisor.colony.{fac}": [11],                             # "This system is now Federation territory."
    "vo.advisor.diplomacy.{fac}": [39],                          # "Diplomatic messages have been received."
    "vo.advisor.intel.{fac}": [42],                              # "Report received from intelligence services."
    "vo.advisor.war.{fac}": [41],                                # "War has been declared."
    "vo.advisor.treaty.broken.{fac}": [40],                      # "A treaty has been broken."
    "vo.advisor.terraform.{fac}": [38],                          # "Terraforming completed."
    "vo.station.{fac}.built": [36],                              # "Outpost constructed."
    "vo.station.{fac}.starbase": [37],                           # "Starbase completed."
    "vo.diplomacy.greet.{fac}": [9],                             # "Greetings from the United Federation of Planets..."
    "vo.diplomacy.{fac}": [9],
    "vo.diplomacy.accept.{fac}": [31],                           # "Treaty will be accepted."
    "vo.diplomacy.break.{fac}": [32],                            # "Treaty will be broken."
    "vo.diplomacy.war.{fac}": [33],                              # "War will be declared."
    "vo.diplomacy.propose.{fac}": [34],                          # "Treaty will be submitted."
    "vo.diplomacy.refuse.{fac}": [35],                           # "Treaty will be rejected."
}

# sfx.snd index -> keys. HEURISTIC (see the module docstring); edit by ear.
SFX = {
    "sfx.ui.click": [69],
    "sfx.ui.beep": [49],
    "sfx.ui.open": [51],
    "sfx.ui.close": [67],
    "sfx.ui.endturn": [50],
    "sfx.ui.turn": [59],
    "sfx.ui.research": [92],
    "sfx.ui.construction": [93],
    # R3.5 (8 Nov 2026): 75 is a run of stepped UI beeps; 132 is the only rising harmonic whoop of the 163 (spectrogram).
    "sfx.alert.red": [132],
    "sfx.warp.in": [1],
    "sfx.warp.out": [0],
    "sfx.cloak.rom": [16],
    "sfx.decloak.rom": [11],
    "sfx.shield.hit": [34, 30],
    "sfx.explosion.small": [33, 39, 36],
    "sfx.explosion.large": [32, 35, 38, 40, 41],
    "sfx.explosion.botf": [32, 35, 38],
    "weapon.generic.beam": [131, 135],
    "weapon.generic.torpedo": [10],  # R3.5: a descending sweep; 146/162 were short blips. Still a guess for Simon's ear.
    "weapon.PulsePhaser": [76, 78, 79],
    "weapon.Disruptor": [133, 134],
}

EOG_TYPES = {"Alli": ["vo.victory.diplomatic.{fac}"], "Anni": ["vo.victory.conquest.{fac}", "vo.victory.win.{fac}"],
             "Domi": ["vo.victory.domination.{fac}"], "Loss": ["vo.victory.defeat.{fac}"]}


NOTE = ("private, generated by tools/build_audio_cues.py; sfx picks are heuristic; R3.5 (8 Nov 2026): sfx.alert.red=sfx_0132 "
        "(rising harmonic whoop, spectrogram match), torpedo=sfx_0010 (descending sweep), beam drops 0132. "
        "Every pick lives in SFX above: edit it there and rerun, never edit cues.json by hand.")


def segment_rms(ffmpeg, path):
    import numpy as np
    raw = subprocess.run([ffmpeg, "-v", "error", "-i", path, "-ac", "1", "-ar", "11025", "-f", "s16le", "-"], capture_output=True).stdout
    d = np.frombuffer(raw, "<i2").astype(float) / 32768
    return float((d ** 2).mean() ** 0.5) if len(d) else 0.0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--audio", required=True, help="LocalAssets/audio")
    ap.add_argument("--ffmpeg", default="ffmpeg")
    ap.add_argument("--check", action="store_true", help="write nothing; exit 1 if the generated cues differ from <audio>/cues.json")
    ap.add_argument("--out", default=None, help="write here instead of <audio>/cues.json (use it to diff against the live file)")
    a = ap.parse_args()
    root = a.audio
    ff = shutil.which(a.ffmpeg) or a.ffmpeg
    cues = {}
    missing = []

    def add(key, rel):
        if os.path.exists(os.path.join(root, rel)):
            cues.setdefault(key, []).append(rel)
        else:
            missing.append(rel)

    # sfx
    for key, idx in SFX.items():
        for i in idx:
            add(key, "sfx/sfx_%04d.wav" % i)
    # voice by converse.bin group
    vg_path = os.path.join(root, "voice", "voice-groups.json")
    if os.path.exists(vg_path):
        clips = json.load(open(vg_path, encoding="utf-8"))["clips"]
        by = {}
        for idx, c in clips.items():
            by.setdefault((c["group"], c["race"]), []).append(int(idx))
        for tpl, groups in VOICE_GROUPS.items():
            for race, fac in RACE_FAC.items():
                for g in groups:
                    for i in sorted(by.get((g, race), [])):
                        add(tpl.replace("{fac}", fac), "voice/english_%04d.ogg" % i)
    else:
        print("no voice-groups.json: voice lines skipped (run botf_voice_groups.py)")
    # end-of-game speeches
    eog = os.path.join(root, "eog")
    if os.path.isdir(eog):
        for f in sorted(os.listdir(eog)):
            m = re.match(r"([CFHKR])EOG(Alli|Anni|Domi|Loss)\.ogg$", f)
            if m:
                for tpl in EOG_TYPES[m[2]]:
                    add(tpl.replace("{fac}", EOG_FAC[m[1]]), "eog/" + f)
    # music
    mdir = os.path.join(root, "music")
    if os.path.isdir(mdir):
        tracks = {}
        for f in sorted(os.listdir(mdir)):
            m = re.match(r"music_t(\d+)_s(\d+)\.ogg$", f)
            if m:
                tracks.setdefault(int(m[1]), []).append(f)
        for f in tracks.get(0, []):
            add("mus.botf.title", "music/" + f)
        cache_path = os.path.join(mdir, "rms.json")
        cache = json.load(open(cache_path)) if os.path.exists(cache_path) else {}
        for race, fac in RACE_FAC.items():
            for f in tracks.get(1 + race, []):
                add("mus.botf.galaxy." + fac, "music/" + f)
            combat = tracks.get(6 + race, [])
            for f in combat:
                if f not in cache:
                    cache[f] = segment_rms(ff, os.path.join(mdir, f))
            ranked = sorted(combat, key=lambda f: cache[f])
            half = len(ranked) // 2
            quiet, loud = set(ranked[:half]), set(ranked[half:])
            for f in combat:   # keep game order inside each half
                if f in quiet:
                    add("mus.botf.combat." + fac, "music/" + f)
                else:
                    add("mus.botf.combat.tension." + fac, "music/" + f)
            for f in tracks.get(11 + race, []):
                add("mus.botf.alt." + fac, "music/" + f)
        json.dump(cache, open(cache_path, "w"))
        # faction-less fallbacks follow the Federation
        for src, dst in (("galaxy", "mus.botf.galaxy"), ("combat", "mus.botf.combat"), ("combat.tension", "mus.botf.combat.tension")):
            cues[dst] = list(cues.get("mus.botf." + src + ".fed", []))
        cues["mus.botf.all"] = list(cues.get("mus.botf.galaxy.fed", []))
    out = a.out or os.path.join(root, "cues.json")
    doc = {"note": NOTE, "cues": cues}
    if a.check:
        live = os.path.join(root, "cues.json")
        have = json.load(open(live, encoding="utf-8"))["cues"] if os.path.exists(live) else {}
        diff = [k for k in sorted(set(cues) | set(have)) if cues.get(k) != have.get(k)]
        print("cues check: %d keys generated, %d in %s, %d differ%s" % (len(cues), len(have), live, len(diff), (": " + ", ".join(diff[:10])) if diff else ""))
        sys.exit(1 if diff else 0)
    json.dump(doc, open(out, "w", encoding="utf-8"), indent=0)
    print("%d keys, %d files referenced, %d missing -> %s" % (len(cues), sum(len(v) for v in cues.values()), len(missing), out))
    if missing:
        print("missing:", missing[:10])


if __name__ == "__main__":
    main()
