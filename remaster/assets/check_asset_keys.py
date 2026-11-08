#!/usr/bin/env python3
"""Check that every ship/weapon asset key the game can ask for has an entry in manifest.json.

  python remaster/assets/check_asset_keys.py

Key scheme (one scheme everywhere: roster, manifest, importer, Unreal client):
  ship.<fac>.<class>      a ship model. Expanded roster classes use the roster's assetKey verbatim
                          (data/base/roster.json). Legacy catalogue classes (data/base/ships.json, ids 0..104) use
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
DATA = os.path.join(ROOT, "data", "base")

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


def _section(name):
    # Content pack section file (docs/DATA-FORMAT.md): {"<name>": [records...]}.
    with open(os.path.join(DATA, name + ".json"), encoding="utf-8") as fh:
        return json.load(fh)[name]


def roster_classes():
    out = []
    weapons = set()
    for c in _section("roster"):
        out.append({"id": c["id"], "key": c["assetKey"], "faction": c["faction"], "role": c["roleName"],
                    "className": c["className"], "hullRole": c["hullRole"], "era": c["era"]})
        weapons.update(w["type"] for w in c["weapons"] if w["count"] > 0)
    return out, sorted(weapons)


def legacy_classes():
    return [{"id": s["id"], "raceId": s["raceId"], "role": s["name"], "className": s["shipClass"],
             "key": legacy_key(s["raceId"], s["shipClass"])} for s in _section("ships")]


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
