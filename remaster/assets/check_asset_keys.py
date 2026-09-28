#!/usr/bin/env python3
"""Check that every ship/weapon asset key the game can ask for has an entry in manifest.json.

  python remaster/assets/check_asset_keys.py

Key scheme (one scheme everywhere: roster, manifest, importer, Unreal client):
  ship.<fac>.<class>      a ship model. Expanded roster classes use the roster's assetKey verbatim
                          (ShipRosterData.h). Legacy catalogue classes (ids 0..104) use
                          ship.<fac>.<slug(className)>, and ship.<fac>.starbase / ship.<fac>.outpost.
  weapon.<Type>           weapon fire/impact audio (and VFX) per roster WeaponType, e.g. weapon.PhaserArray.
  weapon.<Type>.<fac>     optional faction variant (weapon.Disruptor.rom); the client tries it before weapon.<Type>.
  weapon.generic.<beam|torpedo>   last-resort weapon sounds.
  study.<fac>             built-in, in-repo placeholder meshes (project art).
  sfx.* / vo.* / mus.*    sound effects, voice and music cues (AUDIO-PLAN.md).
  vfx.<Type> / vfx.generic.<beam|pulse|torpedo> / vfx.impact.* ...  optional Niagara systems for the rules 3
                          battle scene (BotfCombatTimeline.h); fallback null = procedural effects.

The legacy slug rule is mirrored by botf::assets::legacyShipAssetKey in
remaster/game/Source/BOTFRemake/Private/BotfAssetKeys.h; keep them identical.
Exit status 1 when a key is missing or a fallback points at an unknown key or loops.
"""
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
CORE = os.path.join(ROOT, "game", "Source", "BotfSim")

LEGACY_FACTION = {0: "car", 1: "fed", 2: "fer", 3: "kli", 4: "rom"}
LEGACY_ALIASES = {"ship.borg.cube": 115}  # legacy non-major ids with a manifest key
WEAPON_TYPES = ["PhaseCannon", "PhaserBank", "PhaserArray", "PulsePhaser", "Disruptor", "DisruptorCannon", "PlasmaCannon",
                "PolaronBeam", "CuttingBeam", "EnergyDampener", "TholianWeb", "BioEnergy", "TemporalWeapon", "SpatialTorpedo",
                "PhotonicTorpedo", "PhotonTorpedo", "QuantumTorpedo", "PlasmaTorpedo", "TricobaltDevice", "Torpedo"]


def slug(name):
    s = name.strip().lower().replace("'", "")
    s = re.sub(r"[^a-z0-9]+", "_", s).strip("_")
    return s or "unknown"


def legacy_key(race_id, class_name):
    fac = LEGACY_FACTION.get(race_id)
    if fac is None:
        return None
    return "ship.%s.%s" % (fac, slug(class_name))


def roster_classes():
    text = open(os.path.join(CORE, "ShipRosterData.h"), encoding="utf-8").read()
    rx = re.compile(r'\{(\d+), "(ship\.[^"]+)", Faction::(\w+), (\d+), (\d+), "([^"]*)", "([^"]*)", HullRole::(\w+), \d+, Era::(\w+)')
    out = []
    for m in rx.finditer(text):
        out.append({"id": int(m.group(1)), "key": m.group(2), "faction": m.group(3), "role": m.group(6),
                    "className": m.group(7), "hullRole": m.group(8), "era": m.group(9)})
    weapons = sorted(set(re.findall(r"WeaponType::(\w+), [1-9]", text)))
    return out, weapons


def legacy_classes():
    text = open(os.path.join(CORE, "ReferenceCatalogue.h"), encoding="utf-8").read()
    start = text.index("ShipCatalogue{")
    rx = re.compile(r'\{(\d+), (\d+), "([^"]*)", "([^"]*)"')
    out = []
    for m in rx.finditer(text, start):
        i = int(m.group(1))
        if i > 124:
            break
        out.append({"id": i, "raceId": int(m.group(2)), "role": m.group(3), "className": m.group(4),
                    "key": legacy_key(int(m.group(2)), m.group(4))})
    return out


def required_keys():
    roster, weapons = roster_classes()
    keys = [c["key"] for c in roster]
    keys += [c["key"] for c in legacy_classes() if c["key"]]
    keys += list(LEGACY_ALIASES)
    keys += ["weapon." + w for w in weapons]
    keys += ["weapon.generic.beam", "weapon.generic.torpedo"]
    keys += ["vfx." + w for w in WEAPON_TYPES]
    keys += ["vfx.generic.beam", "vfx.generic.pulse", "vfx.generic.torpedo", "vfx.impact.shield", "vfx.impact.hull",
             "vfx.intercept", "vfx.explosion", "vfx.cloak", "vfx.decloak", "vfx.warp.out"]
    seen = set()
    return [k for k in keys if not (k in seen or seen.add(k))]


def main():
    manifest = json.load(open(os.path.join(HERE, "manifest.json"), encoding="utf-8"))["assets"]
    errors = []
    for k in required_keys():
        if k not in manifest:
            errors.append("missing manifest entry: " + k)
    for k, v in manifest.items():
        seen = [k]
        fb = v.get("fallback")
        while fb:
            if fb not in manifest:
                errors.append("%s: fallback %s is not a manifest key" % (k, fb))
                break
            if fb in seen:
                errors.append("%s: fallback loop %s" % (k, " -> ".join(seen + [fb])))
                break
            seen.append(fb)
            fb = manifest[fb].get("fallback")
        if not re.match(r"^(ship|weapon|study|sfx|vo|mus|vfx)\.", k):
            errors.append("%s: key outside the ship./weapon./study./sfx./vo./mus./vfx. scheme" % k)
    roster, _ = roster_classes()
    if len(roster) < 90:
        errors.append("roster parse found only %d classes" % len(roster))
    for e in errors:
        print("ERROR " + e)
    print("%d required keys, %d manifest entries, %d errors" % (len(required_keys()), len(manifest), len(errors)))
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
