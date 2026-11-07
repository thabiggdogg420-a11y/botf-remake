"""Pure-Python half of blender_hero_ships.py --gltf (no bpy): LOD plan, Godot material hints and the
godot-hero-manifest.json merge. Tools/test_hero_gltf_manifest.py checks it offline.

The hints mirror what Tools/ue_import_hero_ships.py sets on the Unreal material instances, so the Godot build
(remaster/godot/scenes/battle/battle_assets.gd) can give a hero the same look:
  textured slots  (M_ShipHero)    BC x Tint, tangent normal, ORM; MetallicScale 0.35 (x the config metallic when the
                                   slot has no ORM, which also takes RoughnessScale 0.45); EmissiveStrength 1.5, dimmed
                                   by 0.05 / eCoverage (min 0.2) when more than 5% of the emissive map is lit.
  plated slots    (M_ShipPlated)  baked vertex colour "Col" x the tiling faction plating at PanelScale 0.35
                                   (tone R * 0.7 + 0.65, seam 1 - 0.55 G), roughness >= 0.32, metallic 0.35 x metallic.
  plating slots   (MI_Hull_<fac>) no vertex bake: the flat faction hull colour x the same plating.
"""
import json
import math
import os

MANIFEST_NAME = "godot-hero-manifest.json"
MANIFEST_VERSION = 1
SOCKET_PREFIX = "SOCKET_"

# (triangle ratio, screen size) per LOD: the same steps ue_import_hero_ships.py set_lods gives Unreal.
LOD_STEPS = ((1.0, 1.0), (0.5, 0.35), (0.25, 0.15), (0.1, 0.05))
LOD_MIN_TRIANGLES = 2000          # below this a reduced LOD saves nothing worth a mesh
METALLIC_SCALE = 0.35
ROUGHNESS_MIN = 0.32
EMISSIVE_STRENGTH = 1.5
PANEL_SCALE = 0.35
# HullColor of MI_Hull_<fac> (ue_import_generated_ships.py SCHEMES), as blender_hero_ships.py PLATING_TINT.
PLATING_TINT = {"fed": (0.46, 0.48, 0.52), "kli": (0.17, 0.22, 0.17), "rom": (0.20, 0.36, 0.26),
                "car": (0.50, 0.41, 0.27), "fer": (0.52, 0.32, 0.17)}


def lod_plan(slug, triangles, steps=LOD_STEPS, min_triangles=LOD_MIN_TRIANGLES):
    """LODs worth exporting for a hull of `triangles`: [{node, ratio, screen, triangles}], LOD0 first. A step whose
    reduced mesh would fall under min_triangles is dropped (and so are the coarser ones)."""
    out = []
    for i, (ratio, screen) in enumerate(steps):
        tris = int(round(triangles * ratio))
        if i > 0 and tris < min_triangles:
            break
        out.append({"node": "SM_%s" % slug if i == 0 else "SM_%s_LOD%d" % (slug, i), "ratio": ratio,
                    "screen": screen, "triangles": tris})
    return out


def emissive_dim(coverage):
    """ue_import_hero_ships.py: a slot lighting more than ~5% of its map is dimmed in proportion, never below 0.2."""
    return min(1.0, max(0.2, 0.05 / max(float(coverage or 0.0), 1e-4)))


def slot_hints(slot, fac, metallic, bc_tint=None):
    """Godot material hints for one hero.json slot."""
    name = slot.get("name", "M00")
    if slot.get("plating"):
        pfac = slot["plating"]
        baked = bool(slot.get("vertexBake"))
        return {"name": name, "kind": "plated", "faction": pfac, "vertexColor": "Col" if baked else None,
                "tint": None if baked else list(PLATING_TINT.get(pfac, (0.45, 0.45, 0.48))),
                "panelScale": PANEL_SCALE, "tone": [0.7, 0.65] if baked else [0.8, 0.6],
                "seam": 0.55 if baked else 0.6, "roughnessMin": ROUGHNESS_MIN,
                "metallic": round(METALLIC_SCALE * float(slot.get("metallic", metallic)), 4) if baked else 0.5,
                "emissiveStrength": 0.0}
    has_orm = bool(slot.get("orm"))
    hint = {"name": name, "kind": "textured", "faction": fac,
            "tint": [1.0, 1.0, 1.0] if slot.get("bc") else list(slot.get("tint") or (0.6, 0.6, 0.6)),
            # bcTint is already multiplied into the BC bake: recorded for reference, never applied again.
            "bcTint": list(bc_tint) if bc_tint else None,
            "metallicScale": METALLIC_SCALE if has_orm else round(METALLIC_SCALE * float(slot.get("metallic", metallic)), 4),
            "roughnessScale": 1.0 if has_orm else 0.45, "roughnessMin": ROUGHNESS_MIN,
            "emissiveStrength": 0.0, "eCoverage": slot.get("eCoverage")}
    if slot.get("e"):
        hint["emissiveStrength"] = round(EMISSIVE_STRENGTH * emissive_dim(slot.get("eCoverage", 0.0))
                                         * max(1.0, float(slot.get("emissive", 1.0) or 1.0)), 4)
    return hint


def hero_entry(report, glb_rel, lods, bc_tint=None, plating_files=None):
    """The manifest record for one built hero (`report` is the hero.json dict blender_hero_ships.py writes)."""
    fac = report.get("faction") or report["key"].split(".")[1]
    metallic = 0.5
    for s in report.get("slots", []):
        if "metallic" in s:
            metallic = float(s["metallic"])
            break
    slots = [slot_hints(s, fac, metallic, bc_tint) for s in report.get("slots", [])]
    plated = any(s["kind"] == "plated" for s in slots)
    entry = {"glb": glb_rel.replace("\\", "/"), "faction": fac, "lengthMeters": float(report["lengthMeters"]),
             "forward": "+X", "up": "+Y", "units": "m",
             "sockets": list(report.get("sockets", [])), "socketPrefix": SOCKET_PREFIX,
             "lods": lods, "material": "plated" if plated else "textured", "slots": slots,
             "triangles": report.get("triangles"), "source": report.get("source")}
    if plating_files:
        entry["plating"] = {k: v.replace("\\", "/") for k, v in plating_files.items() if v}
    return entry


def merge_manifest(path, entries):
    """Merges {key: entry} into the manifest at `path` (other heroes stay) and writes it. Returns the whole manifest."""
    data = {"version": MANIFEST_VERSION, "heroes": {}}
    if os.path.exists(path):
        try:
            with open(path, encoding="utf-8-sig") as f:
                old = json.load(f)
            if isinstance(old, dict) and isinstance(old.get("heroes"), dict):
                data["heroes"].update(old["heroes"])
        except (OSError, ValueError):
            pass
    data["heroes"].update(entries)
    data["heroes"] = dict(sorted(data["heroes"].items()))
    data["generator"] = "Tools/blender_hero_ships.py --gltf"
    os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=1)
    return data


def lod_distance(length_units, screen, fov_deg=48.0):
    """Camera distance at which a hull `length_units` long covers `screen` of the view height (Unreal's screen size,
    taken as bounding-diameter over view height; 48 degrees is BattleCamera's fov). Godot (HeroLook.lod_distance) sets
    visibility ranges the same way."""
    if screen <= 0:
        return math.inf
    return (length_units * 0.5) / (screen * math.tan(math.radians(fov_deg) * 0.5))
