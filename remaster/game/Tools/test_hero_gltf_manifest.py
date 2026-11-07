"""Offline self-test of hero_gltf_manifest.py (the bpy-free half of blender_hero_ships.py --gltf).

    python3 remaster/game/Tools/test_hero_gltf_manifest.py
"""
import ast
import json
import math
import os
import re
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import hero_gltf_manifest as hgm  # noqa: E402


def read(path):
    with open(path, encoding="utf-8") as f:
        return f.read()


def hero_report(**over):
    r = {"key": "ship.fed.sovereign", "faction": "fed", "file": "SM_sovereign.fbx", "lengthMeters": 685.0,
         "trianglesIn": 900000, "triangles": 160000,
         "sockets": ["WPN_Torp_01", "WPN_Beam_01", "WPN_Beam_02", "ENG_01"],
         "slots": [{"name": "M00", "bc": "T_fed_sovereign_M00_BC.png", "n": "T_fed_sovereign_M00_N.png",
                    "orm": "T_fed_sovereign_M00_ORM.png", "e": "T_fed_sovereign_M00_E.png", "eCoverage": 0.02,
                    "tint": None, "metallic": 0.55, "emissive": 1.0},
                   {"name": "M01", "bc": None, "n": None, "orm": None, "e": None, "tint": [0.3, 0.3, 0.35],
                    "metallic": 0.55, "emissive": 0.0}],
         "source": "fed.sovereign/scene.gltf"}
    r.update(over)
    return r


class LodPlan(unittest.TestCase):
    def test_full_plan_matches_unreal_steps(self):
        plan = hgm.lod_plan("sovereign", 160000)
        self.assertEqual([p["node"] for p in plan], ["SM_sovereign", "SM_sovereign_LOD1", "SM_sovereign_LOD2", "SM_sovereign_LOD3"])
        self.assertEqual([p["ratio"] for p in plan], [1.0, 0.5, 0.25, 0.1])
        self.assertEqual([p["screen"] for p in plan], [1.0, 0.35, 0.15, 0.05])
        self.assertEqual(plan[3]["triangles"], 16000)

    def test_small_hull_drops_useless_lods(self):
        plan = hgm.lod_plan("pod", 6000)
        self.assertEqual([p["node"] for p in plan], ["SM_pod", "SM_pod_LOD1"])  # 3000 kept, 1500 dropped
        self.assertEqual(len(hgm.lod_plan("tiny", 500)), 1)

    def test_lods_off(self):
        self.assertEqual(len(hgm.lod_plan("x", 160000, steps=hgm.LOD_STEPS[:1])), 1)

    def test_lod_distance_grows_as_screen_shrinks(self):
        d = [hgm.lod_distance(10.0, s) for _, s in hgm.LOD_STEPS]
        self.assertTrue(all(a < b for a, b in zip(d, d[1:])))
        self.assertAlmostEqual(d[0], 5.0 / math.tan(math.radians(24.0)), places=6)


class MaterialHints(unittest.TestCase):
    def test_textured_slot(self):
        h = hgm.slot_hints(hero_report()["slots"][0], "fed", 0.55)
        self.assertEqual(h["kind"], "textured")
        self.assertEqual(h["metallicScale"], 0.35)
        self.assertEqual(h["roughnessScale"], 1.0)
        self.assertEqual(h["roughnessMin"], 0.32)
        self.assertEqual(h["tint"], [1.0, 1.0, 1.0])
        self.assertAlmostEqual(h["emissiveStrength"], 1.5)   # 2% lit: not dimmed

    def test_emissive_coverage_dims(self):
        self.assertEqual(hgm.emissive_dim(0.05), 1.0)
        self.assertAlmostEqual(hgm.emissive_dim(0.1), 0.5)
        self.assertAlmostEqual(hgm.emissive_dim(0.46), 0.2)   # the Galor panel map: floored
        slot = dict(hero_report()["slots"][0], eCoverage=0.1, emissive=2.0)
        self.assertAlmostEqual(hgm.slot_hints(slot, "car", 0.45)["emissiveStrength"], 1.5 * 0.5 * 2.0)

    def test_untextured_slot_without_orm(self):
        h = hgm.slot_hints(hero_report()["slots"][1], "fed", 0.55)
        self.assertEqual(h["tint"], [0.3, 0.3, 0.35])
        self.assertAlmostEqual(h["metallicScale"], 0.35 * 0.55)
        self.assertEqual(h["roughnessScale"], 0.45)
        self.assertEqual(h["emissiveStrength"], 0.0)

    def test_plated_slots(self):
        baked = hgm.slot_hints({"name": "M00", "plating": "kli", "vertexBake": True, "metallic": 0.45}, "kli", 0.45)
        self.assertEqual(baked["kind"], "plated")
        self.assertEqual(baked["vertexColor"], "Col")
        self.assertIsNone(baked["tint"])
        self.assertAlmostEqual(baked["metallic"], 0.35 * 0.45)
        self.assertEqual((baked["panelScale"], baked["tone"], baked["seam"], baked["roughnessMin"]), (0.35, [0.7, 0.65], 0.55, 0.32))
        flat = hgm.slot_hints({"name": "M00", "plating": "car"}, "car", 0.45)
        self.assertIsNone(flat["vertexColor"])
        self.assertEqual(flat["tint"], list(hgm.PLATING_TINT["car"]))

    def test_bc_tint_is_recorded_not_applied(self):
        h = hgm.slot_hints(hero_report()["slots"][0], "fer", 0.55, [1.7, 1.5, 1.25])
        self.assertEqual(h["bcTint"], [1.7, 1.5, 1.25])
        self.assertEqual(h["tint"], [1.0, 1.0, 1.0])

    def test_hints_match_the_unreal_importer(self):
        # The Godot look must follow ue_import_hero_ships.py: catch either side drifting.
        ue = read(os.path.join(HERE, "ue_import_hero_ships.py"))
        self.assertIn('"MetallicScale", 0.35)', ue)
        self.assertIn('"RoughnessScale", 0.45)', ue)
        self.assertIn("min(1.0, max(0.2, 0.05 / max(slot.get(\"eCoverage\", 0.0), 1e-4)))", ue)
        self.assertIn('"EmissiveStrength", 1.5 * dim', ue)
        self.assertIn('rough_min.set_editor_property("const_b", 0.32)', ue)
        self.assertIn('"PanelScale", 0.35', ue)
        self.assertRegex(ue, r'"Metallic", 0\.35 \* slot\.get\("metallic"')
        steps = re.search(r"for percent, screen in \(\((.*?)\)\):", ue).group(1)
        self.assertEqual(ast.literal_eval("((" + steps + "))"), hgm.LOD_STEPS)


class Manifest(unittest.TestCase):
    def test_entry(self):
        plan = hgm.lod_plan("sovereign", 160000)
        e = hgm.hero_entry(hero_report(), "ship.fed.sovereign\\SM_sovereign.glb", plan, None,
                           {"panels": "plating/T_HullPanels_fed.png"})
        self.assertEqual(e["glb"], "ship.fed.sovereign/SM_sovereign.glb")
        self.assertEqual(e["lengthMeters"], 685.0)
        self.assertEqual(e["sockets"][0], "WPN_Torp_01")
        self.assertEqual(e["socketPrefix"], "SOCKET_")
        self.assertEqual(e["material"], "textured")
        self.assertEqual([s["name"] for s in e["slots"]], ["M00", "M01"])
        self.assertEqual(len(e["lods"]), 4)
        self.assertEqual(e["plating"], {"panels": "plating/T_HullPanels_fed.png"})
        plated = hgm.hero_entry(hero_report(key="ship.kli.neghvar", faction="kli",
                                            slots=[{"name": "M00", "plating": "kli", "vertexBake": True, "metallic": 0.45}]),
                                "ship.kli.neghvar/SM_neghvar.glb", plan[:1])
        self.assertEqual(plated["material"], "plated")
        self.assertNotIn("plating", plated)

    def test_merge_keeps_other_heroes(self):
        with tempfile.TemporaryDirectory() as d:
            path = os.path.join(d, "ships", hgm.MANIFEST_NAME)
            hgm.merge_manifest(path, {"ship.b": {"glb": "b.glb"}, "ship.a": {"glb": "a.glb"}})
            data = hgm.merge_manifest(path, {"ship.b": {"glb": "b2.glb"}, "ship.c": {"glb": "c.glb"}})
            self.assertEqual(list(data["heroes"]), ["ship.a", "ship.b", "ship.c"])
            on_disk = json.loads(read(path))
            self.assertEqual(on_disk["heroes"]["ship.b"]["glb"], "b2.glb")
            self.assertEqual(on_disk["version"], hgm.MANIFEST_VERSION)

    def test_merge_survives_a_broken_file(self):
        with tempfile.TemporaryDirectory() as d:
            path = os.path.join(d, hgm.MANIFEST_NAME)
            with open(path, "w", encoding="utf-8") as f:
                f.write("{not json")
            self.assertEqual(list(hgm.merge_manifest(path, {"k": {}})["heroes"]), ["k"])


class BlenderScript(unittest.TestCase):
    def test_gltf_is_opt_in(self):
        src = read(os.path.join(HERE, "blender_hero_ships.py"))
        tree = ast.parse(src)
        build = next(n for n in tree.body if isinstance(n, ast.FunctionDef) and n.name == "build")
        defaults = {a.arg: d for a, d in zip(build.args.args[-len(build.args.defaults):], build.args.defaults)}
        self.assertFalse(ast.literal_eval(defaults["gltf"]))
        # The FBX export call itself is untouched by the glTF mode.
        self.assertIn('bpy.ops.export_scene.fbx(filepath=fbx, use_selection=False, apply_unit_scale=True, global_scale=1.0, axis_forward="-Y",', src)
        self.assertLess(src.index("bpy.ops.export_scene.fbx("), src.index("    if gltf:\n        return dict(report"))


if __name__ == "__main__":
    unittest.main(verbosity=1)
