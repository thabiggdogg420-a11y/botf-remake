"""F4 (20 Nov 2026): turns the online print models (Thingiverse STL/OBJ, incoming/online/tv-*) into hullhd sources.

    blender -b --factory-startup --python f4_prep_online.py -- --game <remaster/game> --shots <dir> [--keys k1,k2]

For each key: imports the file(s), joins them, merges coincident vertices, decimates to MAX_TRIS, writes
incoming/f4/<key>/SM_<slug>.obj in Blender's own axes (export forward Y / up Z, so objAxes ["Y", "Z"] reads it back
unchanged) and three workbench views (top, side, front) to <shots>/<key>_{top,side,front}.png, so the nose and top can
be read off and set as "forward" / "up" in the hullhd config. Sources are only read. Private, local only.
"""
import math
import os
import sys

import bpy
from mathutils import Vector

MAX_TRIS = 150000

# key -> files under incoming/online (all joined)
SOURCES = {
    "ship.and.kumari": ["tv-2153244/x/files/kumari.stl"],
    "ship.vul.dkyr": ["tv-2156077/x/files/Vulcan_DKyr.stl"],
    "ship.vul.suurok": ["tv-1540458/x/files/ent_vulcan_suurok_class.STL"],
    "ship.thol.web_spinner": ["tv-2157700/x/files/tholian.stl"],
    "ship.tal.observer": ["tv-2267939/x/files/talarian-3d-complete.stl"],
    "ship.gorn.cruiser": ["tv-3047585/x/files/Gorn_Tuatara_Class.stl"],
    "ship.fer.shuttle": ["tv-5030869/x/files/NAFAR+CLASS.stl"],
    "ship.baj.assault": ["tv-4390840/x/files/Bajoran_Raider.stl"],
}


def arg(name, default=None):
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    return argv[argv.index(name) + 1] if name in argv else default


GAME = arg("--game")
SHOTS = arg("--shots")
ONLY = set(filter(None, (arg("--keys") or "").split(",")))
ONLINE = os.path.join(GAME, "LocalAssets", "incoming", "online")
OUT = os.path.join(GAME, "LocalAssets", "incoming", "f4")


def load(key):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    for rel in SOURCES[key]:
        p = os.path.join(ONLINE, rel)
        if p.lower().endswith(".stl"):
            bpy.ops.wm.stl_import(filepath=p)
        else:
            bpy.ops.wm.obj_import(filepath=p)
    obs = [o for o in bpy.context.scene.objects if o.type == "MESH"]
    bpy.ops.object.select_all(action="DESELECT")
    for o in obs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = obs[0]
    if len(obs) > 1:
        bpy.ops.object.join()
    hull = bpy.context.view_layer.objects.active
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.mesh.remove_doubles(threshold=1e-4)
    bpy.ops.object.mode_set(mode="OBJECT")
    tris = sum(len(p.vertices) - 2 for p in hull.data.polygons)
    if tris > MAX_TRIS:
        m = hull.modifiers.new("dec", "DECIMATE")
        m.ratio = MAX_TRIS / tris
        bpy.ops.object.modifier_apply(modifier=m.name)
    # centre on bounds
    lo = Vector([min(v.co[i] for v in hull.data.vertices) for i in range(3)])
    hi = Vector([max(v.co[i] for v in hull.data.vertices) for i in range(3)])
    c = (lo + hi) / 2
    for v in hull.data.vertices:
        v.co -= c
    bpy.ops.object.shade_smooth_by_angle(angle=math.radians(40))
    return hull, hi - lo, tris


def shots(key, size):
    sc = bpy.context.scene
    sc.render.engine = "BLENDER_WORKBENCH"
    sc.display.shading.light = "STUDIO"
    sc.display.shading.show_cavity = True
    sc.render.resolution_x = sc.render.resolution_y = 640
    sc.render.film_transparent = False
    sc.world = bpy.data.worlds.new("w")
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    sc.collection.objects.link(cam)
    sc.camera = cam
    cam.data.type = "ORTHO"
    cam.data.ortho_scale = max(size) * 1.15
    d = max(size) * 3
    cam.data.clip_end = d * 4
    for name, loc, rot in (("top", (0, 0, d), (0, 0, 0)), ("side", (0, -d, 0), (90, 0, 0)),  # looks +Y: the XZ plane
                           ("front", (d, 0, 0), (90, 0, 90))):
        cam.location = Vector(loc)
        cam.rotation_euler = [math.radians(r) for r in rot]
        sc.render.filepath = os.path.join(SHOTS, "%s_%s.png" % (key, name))
        bpy.ops.render.render(write_still=True)


def main():
    os.makedirs(SHOTS, exist_ok=True)
    for key in SOURCES:
        if ONLY and key not in ONLY:
            continue
        hull, size, tris_in = load(key)
        path = os.path.join(OUT, key, "SM_%s.obj" % key.split(".")[-1])
        os.makedirs(os.path.dirname(path), exist_ok=True)
        bpy.ops.object.select_all(action="DESELECT")
        hull.select_set(True)
        bpy.ops.wm.obj_export(filepath=path, forward_axis="Y", up_axis="Z", export_materials=False, export_uv=False,
                              export_normals=True, export_selected_objects=True)
        tris = sum(len(p.vertices) - 2 for p in hull.data.polygons)
        print("F4PREP %s in %d tris -> %d, size %.1f x %.1f x %.1f" % (key, tris_in, tris, *size))
        shots(key, size)


main()
