"""Blender batch conversion + scale normalisation for BOTF Remastered local ship assets.

Run headless (Blender 4.x):
  blender --background --factory-startup --python blender_normalize.py -- --jobs jobs.json

jobs.json is written by Import-Assets.ps1:
  [{"key": "fed.galaxy", "input": "C:/.../scene.gltf", "output": "C:/.../fed_galaxy.fbx",
    "targetLengthMeters": 642, "scale": null, "sourceForwardAxis": "auto",
    "maxTriangles": 150000, "sockets": ["WPN_Beam_01", ...]}]

Inputs: .gltf/.glb, .obj, .fbx, .3ds (needs the 3DS add-on), .blend, .nif (needs the NifTools
"io_scene_niftools" add-on, https://github.com/niftools/blender_niftools_addon, enabled in the user
preferences; run without --factory-startup in that case, Import-Assets.ps1 does so for .nif).

What it does per job:
  1. Clears the scene and imports the source.
  2. Joins all meshes into one object, applies transforms, recentres the origin on the bounds centre.
  3. Rotates so the ship's nose points +X and up is +Z (Unreal convention). "auto" treats the longest
     horizontal axis as the fore-aft axis and picks the nose as the half with fewer vertices. This
     heuristic is unreliable; check the result in Unreal and set sourceForwardAxis in the manifest
     (or a per-key override in LocalAssets/overrides.json) when it guesses wrong.
  4. Uniformly scales so the longest dimension equals targetLengthMeters (or applies `scale`).
  5. Decimates to maxTriangles if exceeded.
  6. Keeps existing empties named SOCKET_* (or creates placeholder SOCKET_<name> empties at the origin
     for each requested socket, to be positioned by hand in Blender later) and exports FBX. Unreal
     converts SOCKET_-prefixed children to static mesh sockets on import.
  7. Writes <output>.report.json with dimensions, triangle counts and the applied transform.
"""
import json
import math
import os
import sys

import bpy
import mathutils

AXES = {"+X": (0, 1), "-X": (0, -1), "+Y": (1, 1), "-Y": (1, -1), "+Z": (2, 1), "-Z": (2, -1)}


def args():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    if len(argv) >= 2 and argv[0] == "--jobs":
        return argv[1]
    raise SystemExit("usage: blender -b -P blender_normalize.py -- --jobs jobs.json")


def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def import_any(path):
    ext = os.path.splitext(path)[1].lower()
    if ext in (".gltf", ".glb"):
        bpy.ops.import_scene.gltf(filepath=path)
    elif ext == ".obj":
        if hasattr(bpy.ops.wm, "obj_import"):
            bpy.ops.wm.obj_import(filepath=path)
        else:
            bpy.ops.import_scene.obj(filepath=path)
    elif ext == ".fbx":
        bpy.ops.import_scene.fbx(filepath=path)
    elif ext == ".3ds":
        bpy.ops.import_scene.max3ds(filepath=path)
    elif ext == ".nif":
        bpy.ops.import_scene.nif(filepath=path)
    elif ext == ".blend":
        with bpy.data.libraries.load(path, link=False) as (src, dst):
            dst.objects = list(src.objects)
        for ob in dst.objects:
            if ob is not None:
                bpy.context.scene.collection.objects.link(ob)
    else:
        raise RuntimeError("unsupported input " + ext)


def tri_count(ob):
    return sum(len(p.vertices) - 2 for p in ob.data.polygons)


def process(job):
    reset()
    import_any(job["input"])
    meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
    if not meshes:
        raise RuntimeError("no meshes imported")
    sockets_existing = [o for o in bpy.context.scene.objects if o.type == "EMPTY" and o.name.startswith("SOCKET_")]
    socket_world = {o.name: o.matrix_world.translation.copy() for o in sockets_existing}
    # Remove lights, cameras and non-socket empties.
    for o in list(bpy.context.scene.objects):
        if o.type not in ("MESH",):
            bpy.data.objects.remove(o, do_unlink=True)
    bpy.ops.object.select_all(action="DESELECT")
    for o in meshes:
        o.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    bpy.ops.object.parent_clear(type="CLEAR_KEEP_TRANSFORM")
    if len(meshes) > 1:
        bpy.ops.object.join()
    ship = bpy.context.view_layer.objects.active
    ship.name = job["key"].replace(".", "_")
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    tris_in = tri_count(ship)

    # Recentre.
    bb = [mathutils.Vector(c) for c in ship.bound_box]
    lo = mathutils.Vector((min(v.x for v in bb), min(v.y for v in bb), min(v.z for v in bb)))
    hi = mathutils.Vector((max(v.x for v in bb), max(v.y for v in bb), max(v.z for v in bb)))
    centre = (lo + hi) / 2
    ship.data.transform(mathutils.Matrix.Translation(-centre))
    for k in socket_world:
        socket_world[k] -= centre
    dims = hi - lo

    # Orientation: bring source forward to +X (glTF import already converts Y-up to Z-up).
    fwd = job.get("sourceForwardAxis") or "auto"
    if fwd == "auto":
        axis = 0 if dims.x >= dims.y else 1
        # Nose heuristic: the half with fewer vertices along the axis.
        co = [v.co[axis] for v in ship.data.vertices]
        pos = sum(1 for c in co if c > 0)
        sign = 1 if pos < len(co) - pos else -1
    else:
        axis, sign = AXES[fwd]
    target = mathutils.Vector((1, 0, 0))
    src = mathutils.Vector((0, 0, 0))
    src[axis] = sign
    rot = src.rotation_difference(target).to_matrix().to_4x4()
    ship.data.transform(rot)
    for k in socket_world:
        socket_world[k] = rot @ socket_world[k]

    # Scale.
    ship.data.update()
    bb = [mathutils.Vector(c) for c in ship.bound_box]
    length = max(max(v[i] for v in bb) - min(v[i] for v in bb) for i in range(3))
    scale = job.get("scale")
    if not scale:
        tl = job.get("targetLengthMeters")
        scale = (tl / length) if tl and length > 0 else 1.0
    ship.data.transform(mathutils.Matrix.Scale(scale, 4))
    for k in socket_world:
        socket_world[k] *= scale

    # Decimate.
    max_tris = job.get("maxTriangles") or 0
    if max_tris and tri_count(ship) > max_tris:
        mod = ship.modifiers.new("Decimate", "DECIMATE")
        mod.ratio = max(0.01, max_tris / float(tri_count(ship)))
        bpy.context.view_layer.objects.active = ship
        bpy.ops.object.modifier_apply(modifier=mod.name)

    # Sockets.
    made = []
    for name in job.get("sockets") or []:
        sname = "SOCKET_" + name
        e = bpy.data.objects.new(sname, None)
        e.empty_display_size = max(1.0, length * scale * 0.01)
        e.location = socket_world.get(sname, mathutils.Vector((0, 0, 0)))
        bpy.context.scene.collection.objects.link(e)
        e.parent = ship
        made.append({"name": name, "placeholder": sname not in socket_world})

    os.makedirs(os.path.dirname(job["output"]), exist_ok=True)
    bpy.ops.export_scene.fbx(filepath=job["output"], use_selection=False, apply_unit_scale=True, global_scale=1.0,
                             axis_forward="-Y", axis_up="Z", bake_space_transform=False, object_types={"MESH", "EMPTY"},
                             mesh_smooth_type="FACE", path_mode="COPY", embed_textures=False)
    bpy.ops.wm.save_as_mainfile(filepath=os.path.splitext(job["output"])[0] + ".blend")
    report = {"key": job["key"], "input": job["input"], "output": job["output"], "trianglesIn": tris_in,
              "trianglesOut": tri_count(ship), "sourceDims": list(dims), "scale": scale,
              "forward": {"axis": "XYZ"[axis], "sign": sign}, "sockets": made}
    with open(job["output"] + ".report.json", "w") as f:
        json.dump(report, f, indent=1)
    return report


def main():
    jobs = json.load(open(args(), encoding="utf-8-sig"))
    ok = 0
    for job in jobs:
        try:
            r = process(job)
            print("OK  %s  tris %d -> %d  scale %.4f" % (job["key"], r["trianglesIn"], r["trianglesOut"], r["scale"]))
            ok += 1
        except Exception as e:  # keep going; the importer reads the report files
            print("ERR %s  %s" % (job.get("key"), e))
    print("converted %d/%d" % (ok, len(jobs)))


main()
