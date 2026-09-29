"""Lofted ship builder for BOTF Remastered: clean, fitted hulls from a small spec per ship.

Run headless (Blender 4.2+ / 5.x), after blender_generate_ships.py:
  blender --background --factory-startup --python blender_loft_ships.py -- [--specs <dir>] [--out <dir>]
          [--keys ship.kli.*] [--preview] [--views <gemini dir>]

A spec is ArtSource/Ships/<faction>/<slug>.json in ship units (the ship is 1000 long: X 0 stern .. 1000 nose, Y port +,
Z up); it is scaled to the canon length (from generated-ships.json) on export. It holds:
  bodies  lofted hulls: stations [x, half-width, z-top, z-bottom] skinned with superellipse sections ("exponent":
          2 = ellipse, lower = diamond-like, higher = boxy; "segments" around). With "stations": "auto" the width comes
          from the Gemini plan view and the heights from the side view, per station, limited to |Y| <= "band" so
          attached wings are not read as hull ("views" names the renders; see fit_stations).
  parts   kit pieces: wing (root and tip leading/trailing points, thickness), tube (p0, p1, radius, radius1),
          ellipsoid (centre, radii), box (centre, size). "mirror": true adds the starboard copy; "mask" paints it.
  paint   volumes [x0, x1, y0, y1, z0, z1] (mirrored with "mirror") colouring whatever surface falls inside.
  sockets [name, x, y, z, "mirror"?].
Parts keep their own geometry (no voxel remesh), get a subdivision pass for smooth curvature and are joined; UVs are
smart-projected at the kit ships' texel density for the tiling faction plating (M_ShipHull). Output replaces the kit
hull of the same key: <out>/<faction>/<slug>.fbx with the "Mask" colour layer and SOCKET_* empties.
"""
import fnmatch
import json
import math
import os
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector

MASKS = {"hull": (0.0, 0.0, 0.0, 0.0), "windows": (1.0, 0.0, 0.0, 0.0), "warm": (0.0, 1.0, 0.0, 0.0),
         "cool": (0.0, 0.0, 1.0, 0.0), "accent": (0.0, 0.0, 0.0, 1.0)}


# ----------------------------------------------------------------------------------------------------------------
# Geometry kit (each piece is its own bmesh, closed, outward normals)
# ----------------------------------------------------------------------------------------------------------------
def smooth_stations(stations, step):
    """Catmull-Rom resampling of [x, w, zt, zb] stations every ~step along X (ends kept)."""
    st = sorted(stations, key=lambda s: s[0])
    if len(st) < 3:
        return st
    out = []
    for i in range(len(st) - 1):
        p0 = st[max(0, i - 1)]
        p1, p2 = st[i], st[i + 1]
        p3 = st[min(len(st) - 1, i + 2)]
        n = max(1, int(math.ceil((p2[0] - p1[0]) / step)))
        for k in range(n):
            t = k / n
            t2, t3 = t * t, t * t * t
            out.append([0.5 * ((2 * p1[j]) + (-p0[j] + p2[j]) * t + (2 * p0[j] - 5 * p1[j] + 4 * p2[j] - p3[j]) * t2 +
                               (-p0[j] + 3 * p1[j] - 3 * p2[j] + p3[j]) * t3) for j in range(4)])
    out.append(list(st[-1]))
    for s in out:  # overshoot guard
        s[1] = max(0.5, s[1])
        if s[2] - s[3] < 1.0:
            s[2] = s[3] + 1.0
    return out


def section(w, zt, zb, exponent, segments):
    zc, hz = (zt + zb) / 2, (zt - zb) / 2
    pts = []
    for i in range(segments):
        t = math.tau * i / segments
        c, s = math.cos(t), math.sin(t)
        pts.append((w * math.copysign(abs(c) ** (2 / exponent), c), zc + hz * math.copysign(abs(s) ** (2 / exponent), s)))
    return pts


def loft_rings(bm, rings, cap_start=True, cap_end=True):
    """rings: lists of 3D points, same count; skins them and caps the ends with fans."""
    vs = [[bm.verts.new(p) for p in ring] for ring in rings]
    n = len(rings[0])
    for a, b in zip(vs[:-1], vs[1:]):
        for i in range(n):
            bm.faces.new([a[i], a[(i + 1) % n], b[(i + 1) % n], b[i]])
    for ring, flip in ((vs[0], True) if cap_start else (None, None), (vs[-1], False) if cap_end else (None, None)):
        if ring is None:
            continue
        c = bm.verts.new(sum((v.co for v in ring), Vector()) / n)
        for i in range(n):
            f = [ring[i], ring[(i + 1) % n], c]
            bm.faces.new(f[::-1] if not flip else f)
    return [v for ring in vs for v in ring]


def body(bm, stations, exponent, segments, step, plates=None, seed=0):
    rings = []
    for x, w, zt, zb in smooth_stations(stations, step):
        rings.append([Vector((x, y, z)) for y, z in section(w, zt, zb, exponent, segments)])
    loft_rings(bm, rings)
    if plates:
        armour(bm, plates, seed)


def armour(bm, plates, seed):
    """Raised and recessed plates: pairs of neighbouring quads merged into panels, then inset and pushed out or in,
    so the hull reads as armour plating rather than a smooth shell (the Gemini concepts show this at every scale)."""
    import random
    rng = random.Random(seed)
    bm.faces.ensure_lookup_table()
    quads = [f for f in bm.faces if len(f.verts) == 4]
    chosen = [f for f in quads if rng.random() < plates.get("chance", 0.7)]
    if not chosen:
        return
    res = bmesh.ops.inset_individual(bm, faces=chosen, thickness=plates.get("gap", 1.2), depth=0.0, use_even_offset=True)
    for f in chosen:
        if not f.is_valid:
            continue
        d = plates.get("depth", 2.0) * (1 if rng.random() < 0.7 else -0.6) * rng.uniform(0.5, 1.0)
        n = f.normal.copy()
        for v in f.verts:
            v.co += n * d


def airfoil(le, te, thick, up):
    """Hexagonal wing section from leading edge to trailing edge points, thickness along `up`."""
    le, te = Vector(le), Vector(te)
    chord = te - le
    h = up * (thick / 2)
    return [le, le + chord * 0.2 + h, le + chord * 0.75 + h * 0.6, te, le + chord * 0.75 - h * 0.6, le + chord * 0.2 - h]


def wing(bm, spec):
    rle, rte = Vector(spec["root"][0]), Vector(spec["root"][1])
    tle, tte = Vector(spec["tip"][0]), Vector(spec["tip"][1])
    span = ((tle + tte) / 2 - (rle + rte) / 2).normalized()
    chord = (rte - rle).normalized()
    up = chord.cross(span).normalized()
    if up.z < 0:
        up = -up
    t0, t1 = spec.get("thickness", [16, 8])
    n = int(spec.get("sections", 4))
    rings = []
    for k in range(n + 1):
        f = k / n
        rings.append(airfoil(rle.lerp(tle, f), rte.lerp(tte, f), t0 + (t1 - t0) * f, up))
    loft_rings(bm, rings)


def tube(bm, spec):
    p0, p1 = Vector(spec["p0"]), Vector(spec["p1"])
    r0 = spec["radius"]
    r1 = spec.get("radius1", r0)
    seg = int(spec.get("segments", 16))
    axis = (p1 - p0).normalized()
    ref = Vector((0, 0, 1)) if abs(axis.z) < 0.9 else Vector((1, 0, 0))
    u = axis.cross(ref).normalized()
    v = axis.cross(u).normalized()
    profile = spec.get("profile", [[0, 1], [1, 1]])  # [fraction along, radius factor]
    rings = []
    for f, k in profile:
        c = p0.lerp(p1, f)
        r = (r0 + (r1 - r0) * f) * k
        rings.append([c + (u * math.cos(math.tau * i / seg) + v * math.sin(math.tau * i / seg)) * r for i in range(seg)])
    loft_rings(bm, rings)


def ellipsoid(bm, spec):
    c, r = Vector(spec["centre"]), Vector(spec["radii"])
    ret = bmesh.ops.create_uvsphere(bm, u_segments=int(spec.get("segments", 24)), v_segments=12, radius=1.0)
    for v in ret["verts"]:
        v.co = Vector((c.x + v.co.x * r.x, c.y + v.co.y * r.y, c.z + v.co.z * r.z))


def box(bm, spec):
    c, s = Vector(spec["centre"]), Vector(spec["size"])
    ret = bmesh.ops.create_cube(bm, size=1.0)
    for v in ret["verts"]:
        v.co = Vector((c.x + v.co.x * s.x, c.y + v.co.y * s.y, c.z + v.co.z * s.z))


KIT = {"wing": wing, "tube": tube, "ellipsoid": ellipsoid, "box": box}


def to_object(name, bm, mask, subdiv):
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-4)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    col = bm.loops.layers.color.new("Mask")
    for f in bm.faces:
        for loop in f.loops:
            loop[col] = MASKS[mask]
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    if subdiv:
        m = ob.modifiers.new("subdiv", "SUBSURF")
        m.levels = subdiv
        m.render_levels = subdiv
    return ob


def apply_all(ob):
    bpy.context.view_layer.objects.active = ob
    for m in list(ob.modifiers):
        bpy.ops.object.modifier_apply(modifier=m.name)


def mirrored(ob):
    m = ob.modifiers.new("mirror", "MIRROR")
    m.use_axis = (False, True, False)
    m.use_bisect_axis = (False, False, False)
    m.use_clip = False


# ----------------------------------------------------------------------------------------------------------------
# Fitting stations to Gemini's orthographic renders
# ----------------------------------------------------------------------------------------------------------------
def load_mask(path, threshold=0.06):
    import numpy as np
    img = bpy.data.images.load(path, check_existing=False)
    w, h = img.size
    px = np.array(img.pixels[:], dtype=np.float32).reshape(h, w, img.channels)[::-1, :, :3]
    bpy.data.images.remove(img)
    mask = px.max(axis=2) > threshold
    ys, xs = np.nonzero(mask)
    return mask, (xs.min(), ys.min(), xs.max() + 1, ys.max() + 1)


def fit_stations(spec_body, views, count=28):
    """Stations from the plan (half-width of the run through the centreline, within the band) and the side view
    (top and bottom of the run at mid-height), sampled at `count` stations over [from, to] (ship units)."""
    import numpy as np
    tm, tb = views["top"]
    sm, sb = views["side"]
    L = 1000.0
    beam = L * (tb[3] - tb[1]) / (tb[2] - tb[0])
    height = L * (sb[3] - sb[1]) / (sb[2] - sb[0])
    x0, x1 = spec_body.get("from", 0), spec_body.get("to", 1000)
    band = spec_body.get("band", [[0, beam], [1000, beam]])
    out = []
    for k in range(count + 1):
        x = x0 + (x1 - x0) * k / count
        x = min(max(x, 1.0), L - 1.0)
        # plan: column at x, run through the centre row
        col = int(tb[0] + x / L * (tb[2] - tb[0]))
        ccol = tm[tb[1]:tb[3], min(col, tm.shape[1] - 1)]
        mid = len(ccol) // 2
        if not ccol[mid]:
            continue
        up = mid
        while up > 0 and ccol[up - 1]:
            up -= 1
        dn = mid
        while dn < len(ccol) - 1 and ccol[dn + 1]:
            dn += 1
        w = (dn - up + 1) / 2 / len(ccol) * beam
        limit = np.interp(x, [b[0] for b in band], [b[1] for b in band])
        w = min(w, limit)
        # side: column at x, the run through the hull's mid-height
        scol = int(sb[0] + x / L * (sb[2] - sb[0]))
        s = sm[sb[1]:sb[3], min(scol, sm.shape[1] - 1)]
        rows = np.nonzero(s)[0]
        if not len(rows):
            continue
        zc_row = int(np.median(rows))
        a = zc_row
        while a > 0 and s[a - 1]:
            a -= 1
        b = zc_row
        while b < len(s) - 1 and s[b + 1]:
            b += 1
        zt = (0.5 - a / len(s)) * height
        zb = (0.5 - (b + 1) / len(s)) * height
        out.append([x, w, zt, zb])
    return out


# ----------------------------------------------------------------------------------------------------------------
# Build and export
# ----------------------------------------------------------------------------------------------------------------
def build(spec, views):
    style = spec.get("style", {})
    objs = []
    for b in spec.get("bodies", []):
        stations = b["stations"]
        if stations == "auto":
            stations = fit_stations(b, views)
        bm = bmesh.new()
        body(bm, stations, b.get("exponent", style.get("exponent", 2.0)), int(b.get("segments", style.get("segments", 16))),
             b.get("step", style.get("step", 12.0)), b.get("plates", style.get("plates")), seed=len(objs) + 1)
        objs.append((to_object(b["name"], bm, b.get("mask", "hull"), b.get("subdiv", style.get("subdiv", 1))), b.get("mirror", False)))
    for i, p in enumerate(spec.get("parts", [])):
        bm = bmesh.new()
        KIT[p["type"]](bm, p)
        default_sub = 1 if p["type"] == "wing" else 0
        objs.append((to_object("%s_%d" % (p["type"], i), bm, p.get("mask", "hull"), p.get("subdiv", default_sub)), p.get("mirror", False)))
    for ob, mirror in objs:
        if mirror:
            mirrored(ob)
        apply_all(ob)
    bpy.ops.object.select_all(action="DESELECT")
    for ob, _ in objs:
        ob.select_set(True)
    bpy.context.view_layer.objects.active = objs[0][0]
    bpy.ops.object.join()
    hull = objs[0][0]
    hull.name = "Hull"
    return hull


def paint(hull, volumes):
    me = hull.data
    col = me.color_attributes["Mask"]
    for vol in volumes:
        x0, x1, y0, y1, z0, z1 = vol["box"]
        target = MASKS[vol["mask"]]
        mirror = vol.get("mirror", False)
        facing = vol.get("facing")  # "side" (windows on flanks), "down" (undersides), "up"
        hit = set()
        for v in me.vertices:
            x, y, z = v.co
            nz = v.normal.z
            if (facing == "side" and abs(nz) > 0.3) or (facing == "down" and nz > -0.35) or (facing == "up" and nz < 0.35):
                continue
            for yy in ((y, -y) if mirror else (y,)):
                if x0 <= x <= x1 and y0 <= yy <= y1 and z0 <= z <= z1:
                    hit.add(v.index)
                    break
        for loop in me.loops:
            if loop.vertex_index in hit:
                col.data[loop.index].color = target


def finish(hull, length):
    bpy.context.view_layer.objects.active = hull
    bpy.ops.object.select_all(action="DESELECT")
    hull.select_set(True)
    bpy.ops.object.shade_smooth_by_angle(angle=math.radians(35))
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.uv.smart_project(angle_limit=math.radians(55), island_margin=0.002)
    bpy.ops.object.mode_set(mode="OBJECT")
    m = Matrix.Scale(length / 1000.0, 4) @ Matrix.Translation(Vector((-500.0, 0, 0)))
    hull.data.transform(m)
    me = hull.data
    uv = me.uv_layers.active.data
    area3d = sum(p.area for p in me.polygons)
    area_uv = 0.0
    for p in me.polygons:
        pts = [uv[i].uv for i in p.loop_indices]
        area_uv += abs(sum(pts[i].x * pts[i - 1].y - pts[i - 1].x * pts[i].y for i in range(len(pts)))) / 2
    s = math.sqrt(area3d / max(area_uv, 1e-9)) / max(4.0, length / 14.0)
    for d in uv:
        d.uv = d.uv * s
    return m


def export(spec_path, views_dir, out_dir, lengths, preview_dir):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    spec = json.load(open(spec_path, encoding="utf-8-sig"))
    key = spec["key"]
    fac, slug = key.split(".")[1], key.split(".", 2)[2]
    views = {}
    if any(b.get("stations") == "auto" for b in spec.get("bodies", [])):
        for v in ("side", "top"):
            path = os.path.join(views_dir, fac, "%s-%s.png" % (slug, v))
            if not os.path.exists(path):
                path = path[:-4] + ".jpg"
            views[v] = load_mask(path)
    hull = build(spec, views)
    paint(hull, spec.get("paint", []))
    length = float(spec.get("length", lengths.get(key, 400.0)))
    m = finish(hull, length)
    names = []
    for entry in spec.get("sockets", []):
        name, x, y, z = entry[:4]
        mirror = len(entry) > 4 and entry[4] == "mirror"
        for sy in ((y, -y) if mirror and y else (y,)):
            e = bpy.data.objects.new("SOCKET_" + name, None)
            e.location = m @ Vector((x, sy, z))
            e.empty_display_size = length * 0.01
            bpy.context.scene.collection.objects.link(e)
            e.parent = hull
            names.append(name)
    tris = sum(len(p.vertices) - 2 for p in hull.data.polygons)
    path = os.path.join(out_dir, fac, slug + ".fbx")
    os.makedirs(os.path.dirname(path), exist_ok=True)
    bpy.ops.export_scene.fbx(filepath=path, use_selection=False, apply_unit_scale=True, global_scale=1.0, axis_forward="-Y", axis_up="Z",
                             object_types={"MESH", "EMPTY"}, mesh_smooth_type="FACE", colors_type="LINEAR", use_mesh_modifiers=True,
                             add_leaf_bones=False, bake_space_transform=False)
    if preview_dir:
        for tag, cam in (("", (0.85, -1.05, 0.6)), ("-side", (0.0, -2.2, 0.0)), ("-top", (0.0, -0.001, 2.2)), ("-front", (2.2, 0.0, 0.0))):
            render_preview(hull, length, os.path.join(preview_dir, key + ".loft%s.png" % tag), cam, ortho=bool(tag))
    return {"key": key, "file": os.path.relpath(path, out_dir).replace("\\", "/"), "lengthMeters": length, "triangles": tris,
            "archetype": "loft", "sockets": names}


def render_preview(ob, length, path, cam_dir, ortho=False):
    scene = bpy.context.scene
    if "Preview" not in bpy.data.materials:
        mat = bpy.data.materials.new("Preview")
        if mat.node_tree is None:
            mat.use_nodes = True
        nt = mat.node_tree
        bsdf = nt.nodes.get("Principled BSDF")
        attr = nt.nodes.new("ShaderNodeVertexColor")
        attr.layer_name = "Mask"
        sep = nt.nodes.new("ShaderNodeSeparateColor")
        nt.links.new(attr.outputs["Color"], sep.inputs["Color"])
        bsdf.inputs["Base Color"].default_value = (0.3, 0.36, 0.32, 1)
        bsdf.inputs["Metallic"].default_value = 0.5
        bsdf.inputs["Roughness"].default_value = 0.45
        emit = nt.nodes.new("ShaderNodeCombineColor")
        nt.links.new(sep.outputs["Green"], emit.inputs["Red"])
        nt.links.new(sep.outputs["Red"], emit.inputs["Green"])
        nt.links.new(sep.outputs["Blue"], emit.inputs["Blue"])
        nt.links.new(emit.outputs["Color"], bsdf.inputs["Emission Color"])
        bsdf.inputs["Emission Strength"].default_value = 3.0
        ob.data.materials.clear()
        ob.data.materials.append(mat)
        for rot, energy in (((math.radians(40), 0, math.radians(30)), 4.0), ((math.radians(-60), 0, math.radians(200)), 1.5)):
            light = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
            light.data.energy = energy
            light.rotation_euler = rot
            scene.collection.objects.link(light)
        world = bpy.data.worlds.new("W")
        world.color = (0.02, 0.02, 0.03)
        scene.world = world
    cam = scene.camera
    if cam is None:
        cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
        scene.collection.objects.link(cam)
        scene.camera = cam
    cam.location = Vector(cam_dir) * length
    cam.rotation_euler = (Vector((0, 0, 0)) - cam.location).to_track_quat("-Z", "Y").to_euler()
    cam.data.type = "ORTHO" if ortho else "PERSP"
    cam.data.ortho_scale = length * 1.25
    cam.data.lens = 50
    cam.data.clip_start = length * 0.01
    cam.data.clip_end = length * 20
    engines = [x.identifier for x in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items]
    scene.render.engine = "BLENDER_EEVEE_NEXT" if "BLENDER_EEVEE_NEXT" in engines else "BLENDER_EEVEE"
    scene.render.resolution_x = 1280
    scene.render.resolution_y = 720
    scene.render.filepath = path
    bpy.ops.render.render(write_still=True)


def arg(name, default=None):
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    if name in argv:
        i = argv.index(name)
        return argv[i + 1] if i + 1 < len(argv) and not argv[i + 1].startswith("--") else True
    return default


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    specs_dir = os.path.abspath(arg("--specs", os.path.join(here, "..", "ArtSource", "Ships")))
    views_dir = os.path.abspath(arg("--views", os.path.join(here, "..", "ArtSource", "Gemini")))
    out_dir = os.path.abspath(arg("--out", os.path.join(here, "..", "ArtSource", "Generated", "Ships")))
    listing = os.path.join(out_dir, "generated-ships.json")
    ships = {e["key"]: e for e in json.load(open(listing, encoding="utf-8"))["ships"]} if os.path.exists(listing) else {}
    lengths = {k: e["lengthMeters"] for k, e in ships.items()}
    preview_dir = os.path.join(out_dir, "previews") if arg("--preview") else None
    if preview_dir:
        os.makedirs(preview_dir, exist_ok=True)
    pats = arg("--keys").split(",") if isinstance(arg("--keys"), str) else None
    specs = sorted(os.path.join(dp, f) for dp, _, fs in os.walk(specs_dir) for f in fs if f.endswith(".json"))
    done = 0
    for sp in specs:
        key = "ship.%s.%s" % (os.path.basename(os.path.dirname(sp)), os.path.splitext(os.path.basename(sp))[0])
        if pats and not any(fnmatch.fnmatch(key, p) for p in pats):
            continue
        try:
            r = export(sp, views_dir, out_dir, lengths, preview_dir)
            ships[r["key"]] = r
            done += 1
            print("OK  %-32s loft %6d tris  %d sockets" % (r["key"], r["triangles"], len(r["sockets"])))
        except Exception as e:
            import traceback
            traceback.print_exc()
            print("ERR %s  %s" % (sp, e))
    with open(listing, "w", encoding="utf-8") as f:
        json.dump({"ships": sorted(ships.values(), key=lambda e: e["key"])}, f, indent=1)
    print("loft ships %d/%d" % (done, len(specs)))


main()
