"""F4 (20 Nov 2026): the hullhd build config for the classes that still drew a procedural hull in a normal game.

    python f4_config.py --game <main tree remaster/game> --out f4-ships.json

Three kinds of source, all built by hullhd_build.py (orient, canon length, bake, glb, manifest merge):
  * modelled  - original hulls from f4_blender_builds.py (incoming/f4/<key>/SM_<slug>.obj, nose +X top +Z);
  * online    - free print models from Thingiverse made into OBJs by f4_prep_online.py (Blender axes; the nose and
                top per key below, read off the prep views); licences in docs/ASSET-SOURCES.md;
  * library   - Bridge Commander library ships (textured), added through make_config.py --trial.
Modelled and online hulls are untextured: hullhd bakes the faction plating ("plating": <fac>, PLATING_BAKE).
"""
import argparse, json, os, re, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))

# key -> (plating faction, forward, up)
MODELLED = {
    "ship.xindi.reptilian": ("xin", "+X", "+Z"), "ship.xindi.insectoid": ("xin", "+X", "+Z"),
    "ship.xindi.aquatic": ("xin", "+X", "+Z"), "ship.xindi.primate": ("xin", "+X", "+Z"),
    "ship.xindi.weapon": ("xin", "+X", "+Z"), "ship.sul.cell_ship": ("sul", "+X", "+Z"),
    "ship.nau.raider": ("nau", "+X", "+Z"), "ship.baj.lightship": ("baj", "+X", "+Z"),
    "ship.fer.outpost": ("fer", "+X", "+Z"), "ship.fer.starbase": ("fer", "+X", "+Z"),
    "ship.fer.pod": ("fer", "+X", "+Z"), "ship.fer.scout": ("fer", "+X", "+Z"),
}
ONLINE = {
    "ship.and.kumari": ("and", "-Y", "+Z"), "ship.vul.dkyr": ("vul", "-Y", "+Z"),
    "ship.vul.suurok": ("vul", "-Y", "+Z"), "ship.thol.web_spinner": ("thol", "-Y", "+Z"),
    "ship.tal.observer": ("tal", "-X", "+Z"), "ship.gorn.cruiser": ("gor", "+Z", "+Y"),
    "ship.fer.shuttle": ("fer", "+Y", "+Z"), "ship.baj.assault": ("baj", "+Y", "+Z"),
}
# key -> Bridge Commander library id
LIBRARY = {
    "ship.borg.cube": "B_TNG_CUBE", "ship.car.atr4107": "CardFreighter",
    "ship.car.scout": "C_TNG_BARKUS", "ship.rom.drone": "R_TOS_SWARM",
}

ap = argparse.ArgumentParser()
ap.add_argument("--game", required=True)
ap.add_argument("--out", required=True)
ap.add_argument("--only", default="")
a = ap.parse_args()

t = open(os.path.join(HERE, "..", "..", "godot", "scenes", "battle", "ship_factory.gd"), encoding="utf-8").read()
b = t[t.index("const CANON_LENGTHS"):]; b = b[:b.index("}")]
canon = {k: int(v) for k, v in re.findall(r'"(ship\.[a-z0-9_.]+)":\s*(\d+)', b)}

ships = []
for group in (MODELLED, ONLINE):
    for key, (fac, fwd, up) in group.items():
        ships.append({"key": key, "role": "f4", "source": "f4/%s/SM_%s.obj" % (key, key.split(".")[-1]),
                      "objAxes": ["Y", "Z"], "forward": fwd, "up": up, "lengthMeters": canon[key], "plating": fac,
                      "faction": key.split(".")[1], "metallic": 0.3})
lib = os.path.join(os.path.dirname(os.path.abspath(a.out)), "f4-library.json")
subprocess.check_call([sys.executable, os.path.join(HERE, "make_config.py"), "--game", a.game, "--out", lib,
                       "--trial", ",".join("%s=%s" % kv for kv in LIBRARY.items()), "--only", ",".join(LIBRARY)])
for s in json.load(open(lib))["ships"]:
    s["lengthMeters"] = s.get("lengthMeters") or canon[s["key"]]
    ships.append(s)
if a.only:
    keep = set(a.only.split(","))
    ships = [s for s in ships if s["key"] in keep]
json.dump({"defaults": {"maxTriangles": 160000, "metallic": 0.55}, "ships": ships}, open(a.out, "w"), indent=1)
print("f4 config %s: %d ships" % (a.out, len(ships)))
