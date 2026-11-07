"""Stages the Bridge Commander hero sources and writes hero-ships.json from ArtSource/HeroShips/bc-picks.json.

    python stage_bc_heroes.py [--check]

For each pick, copies the converted BC ship (Starhelm's H:/StarhelmTools/bc-ships/<id>: SM_<id>.fbx, materials.json,
textures) into LocalAssets/incoming/bc/<id> (gitignored; private, never distributed). Where the Starhelm HD pass made
Real-ESRGAN 4x maps (bc-ships-hd/<id>/<stem>_basecolor_4x.png), those replace the 1x base colour and emissive.
Weapon and engine sockets come from the ship's BC hardpoints (phasers -> WPN_Beam_NN, pulse cannons -> WPN_Pulse_NN,
torpedo tubes -> WPN_Torp_NN, engine glows -> ENG_NN) as fractions of the hull length about the hull's bounds centre,
nose +X, top +Z: the frame blender_hero_ships.py works in. Then blender_hero_ships.py builds them as usual.
--check only reports what would be staged.
"""
import json
import os
import shutil
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
GAME = os.path.normpath(os.path.join(HERE, ".."))
PICKS = os.path.join(GAME, "ArtSource", "HeroShips", "bc-picks.json")
CONFIG = os.path.join(GAME, "ArtSource", "HeroShips", "hero-ships.json")
INCOMING = os.path.join(GAME, "LocalAssets", "incoming", "bc")
SOCKET_KINDS = {"PhaserProperty": ("WPN_Beam", 12), "PulseWeaponProperty": ("WPN_Pulse", 8),
                "TorpedoTubeProperty": ("WPN_Torp", 8), "EngineProperty": ("ENG", 6)}


def load(path):
    with open(path, encoding="utf-8-sig") as f:
        return json.load(f)


def sockets_for(src):
    """[[name, x, y, z], ...] as fractions of the length, nose +X, about the bounds centre; [] without hardpoints."""
    hp_path = os.path.join(src, "hardpoints.json")
    if not os.path.isfile(hp_path):
        return []
    hp, ship, build = load(hp_path), load(os.path.join(src, "ship.json")), load(os.path.join(src, "build.json"))
    nif_per_gu = float(hp.get("fit", {}).get("best_nif_per_gu") or ship["frame"].get("nif_per_gu", 100.0))
    m_per_nif = float(ship["frame"].get("metres_per_nif", 1.0))
    lo, hi = build["bbox_blender_m"]["lo"], build["bbox_blender_m"]["hi"]
    centre = [(a + b) / 2 for a, b in zip(lo, hi)]
    length = max(hi[0] - lo[0], 1e-6)
    out, counts = [], {}
    for rec in hp.get("records", []):
        kind = SOCKET_KINDS.get(rec.get("kind"))
        pos = rec.get("Position")
        if not kind or not pos or len(pos) < 3:
            continue
        prefix, cap = kind
        n = counts.get(prefix, 0) + 1
        if n > cap:
            continue
        counts[prefix] = n
        x, y, z = (float(v) * nif_per_gu * m_per_nif for v in pos[:3])  # BC: x starboard, y forward, z up
        bl = (y, -x, z)  # Blender: +X forward, +Y port, +Z up
        out.append(["%s_%02d" % (prefix, n)] + [round((bl[i] - centre[i]) / length, 4) for i in range(3)])
    return out


def stage(pick, bc_root, hd_root, check):
    bid = pick["bc"]
    src = os.path.join(bc_root, bid)
    fbx = os.path.join(src, "SM_%s.fbx" % bid)
    if not os.path.isfile(fbx):
        raise FileNotFoundError(fbx)
    dst = os.path.join(INCOMING, bid)
    mats = load(os.path.join(src, "materials.json")).get("materials", [])
    hd = os.path.join(hd_root, bid)
    maps = {}
    for m in mats:
        files = m.get("maps", {})
        entry = {}
        for kind, field in (("bc", "basecolor"), ("e", "emissive")):
            name = files.get(field)
            if not name:
                continue
            stem, ext = os.path.splitext(name)
            hd_file = os.path.join(hd, stem + "_4x" + ext)
            if os.path.isfile(hd_file):
                entry[kind] = ("hd", hd_file, "textures/" + stem + "_4x" + ext)
            elif os.path.isfile(os.path.join(src, "textures", name)):
                entry[kind] = ("1x", os.path.join(src, "textures", name), "textures/" + name)
        if entry:
            maps[m["name"]] = entry
    if not check:
        os.makedirs(os.path.join(dst, "textures"), exist_ok=True)
        shutil.copy2(fbx, os.path.join(dst, os.path.basename(fbx)))
        for entry in maps.values():
            for _, path, rel in entry.values():
                shutil.copy2(path, os.path.join(dst, rel))
    hd_used = sum(1 for e in maps.values() for v in e.values() if v[0] == "hd")
    ship = {"key": pick["key"], "source": "bc/%s/SM_%s.fbx" % (bid, bid), "forward": "+X", "up": "+Z",
            "lengthMeters": pick["lengthMeters"], "metallic": pick.get("metallic", 0.5),
            "bcMaps": {name: {k: v[2] for k, v in e.items()} for name, e in maps.items()}, "bcShip": bid}
    for extra in ("bcTint", "variant", "maxTriangles"):
        if extra in pick:
            ship[extra] = pick[extra]
    sockets = sockets_for(src)
    if sockets:
        ship["sockets"] = sockets
    print("BC %-22s <- %-24s %d slots (%d HD maps), %d sockets" % (pick["key"], bid, len(maps), hd_used, len(sockets)))
    return ship


def main():
    check = "--check" in sys.argv
    picks = load(PICKS)
    config = load(CONFIG)
    ships = [stage(p, picks["bcRoot"], picks["hdRoot"], check) for p in picks["picks"]]
    if check:
        return
    config["_comment"] = ("Hero ships, written by Tools/stage_bc_heroes.py from bc-picks.json (Bridge Commander models "
                          "converted by the Starhelm pipeline; private, local only). Sources are paths under "
                          "LocalAssets/incoming (gitignored); nothing here is a binary. lengthMeters is canon length. "
                          "Tools/blender_hero_ships.py builds them.")
    config["ships"] = ships
    with open(CONFIG, "w", encoding="utf-8") as f:
        json.dump(config, f, indent=1)
        f.write("\n")
    print("hero-ships.json: %d ships" % len(ships))


if __name__ == "__main__":
    main()
