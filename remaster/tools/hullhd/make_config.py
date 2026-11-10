"""Writes the Blender build config for the hullhd lane.

  python make_config.py --game <main tree remaster/game> --out hullhd-ships.json [--only key,key] [--trial key=BcId,...]

Targets: every entry of hullhd_picks.json (new + swap) and a rebuild of every existing hero whose manifest source is a
Bridge Commander library ship (same model, HD textures). For each ship the config carries the absolute paths of the
base colour / emissive / normal map per material: the Real-ESRGAN cache of H:/StarhelmTools/bc-ships-hd/_cache (keyed by
the SHA-256 of the 1x texture) where it has the texture, else the 1x original. Library files are only read.
Prints the ships whose textures are missing from the cache (feed those to upscale_ships_bulk.py)."""
import argparse, hashlib, importlib.util, json, os, re, sys

LIB = "H:/StarhelmTools/bc-ships"
CACHE = "H:/StarhelmTools/bc-ships-hd/_cache"
HERE = os.path.dirname(os.path.abspath(__file__))

ap = argparse.ArgumentParser()
ap.add_argument("--game", required=True)
ap.add_argument("--out", required=True)
ap.add_argument("--only")
ap.add_argument("--trial", default="")
ap.add_argument("--tex", type=int, default=2048)
a = ap.parse_args()
game = a.game.replace("\\", "/")

spec = importlib.util.spec_from_file_location("stage", game + "/Tools/stage_bc_heroes.py")
stage = importlib.util.module_from_spec(spec); spec.loader.exec_module(stage)

canon = {}
t = open(HERE + "/../../godot/scenes/battle/ship_factory.gd", encoding="utf-8").read()
b = t[t.index("const CANON_LENGTHS"):]; b = b[:b.index("}")]
canon = {k: int(v) for k, v in re.findall(r'"(ship\.[a-z0-9_.]+)":\s*(\d+)', b)}

manifest = json.load(open(game + "/LocalAssets/ships/godot-hero-manifest.json"))["heroes"]
picks = {p["key"]: p for p in json.load(open(game + "/ArtSource/HeroShips/bc-picks.json"))["picks"]}
mine = json.load(open(HERE + "/hullhd_picks.json"))
index = json.load(open(CACHE + "/index.json"))["entries"]

targets = {}   # key -> (bc id, role, note)
for grp in ("new", "swap"):
    for k, (bc, note) in mine[grp].items():
        targets[k] = (bc, grp, note)
for k, h in manifest.items():
    if k in targets:
        continue
    m = re.match(r"bc/([^/]+)/SM_", h.get("source", ""))
    if m:
        targets[k] = (m.group(1), "rebuild", "")
for kv in filter(None, a.trial.split(",")):
    k, bc = kv.split("=")
    targets[k] = (bc, "trial", "")
if a.only:
    keep = set(a.only.split(","))
    targets = {k: v for k, v in targets.items() if k in keep}


def sha(p):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for c in iter(lambda: f.read(1 << 20), b""):
            h.update(c)
    return h.hexdigest()


ships, missing = [], {}
for key, (bc, role, note) in sorted(targets.items()):
    src = "%s/%s" % (LIB, bc)
    mats = json.load(open(src + "/materials.json")).get("materials", [])
    maps, hd, one = {}, 0, 0
    for m in mats:
        files = m.get("maps", {}); e = {}
        for kind, field in (("bc", "basecolor"), ("e", "emissive")):
            name = files.get(field)
            if not name:
                continue
            p = "%s/textures/%s" % (src, name)
            if not os.path.isfile(p):
                continue
            ent = index.get("%s:%s" % (sha(p), field))
            if ent and ent.get("color"):
                c = ent["color"]; c = c if os.path.isabs(c) else CACHE + "/" + c
                if os.path.isfile(c):
                    e[kind] = c.replace("\\", "/"); hd += 1
                    if kind == "bc" and ent.get("normal"):
                        n = ent["normal"]; n = n if os.path.isabs(n) else CACHE + "/" + n
                        if os.path.isfile(n):
                            e["n"] = n.replace("\\", "/")
                    continue
            e[kind] = p; one += 1
            if kind == "bc":
                missing.setdefault(bc, 0); missing[bc] += 1
        if e:
            maps[m["name"]] = e
    pk = picks.get(key, {})
    base = key[:-2] if key.endswith("_t") else key
    length = canon.get(base) or manifest.get(base, {}).get("lengthMeters") or pk.get("lengthMeters")
    s = {"key": key, "bcShip": bc, "role": role, "note": note, "source": "%s/SM_%s.fbx" % (src, bc), "forward": "+X", "up": "+Z",
         "lengthMeters": length, "metallic": pk.get("metallic", 0.55), "bcMaps": maps, "maxTex": a.tex,
         "hdMaps": hd, "oneXMaps": one}
    for extra in ("bcTint", "variant", "maxTriangles"):
        if extra in pk and role == "rebuild":
            s[extra] = pk[extra]
    so = stage.sockets_for(src)
    if so:
        s["sockets"] = so
    ships.append(s)
json.dump({"defaults": {"maxTriangles": 160000, "metallic": 0.55}, "ships": ships}, open(a.out, "w"), indent=1)
print("config %s: %d ships (%d new, %d swap, %d rebuild, %d trial)" % (a.out, len(ships), *(sum(1 for s in ships if s["role"] == r) for r in ("new", "swap", "rebuild", "trial"))))
print("ships with 1x-only base maps (not in the HD cache):", missing)
