"""Blueprint ship builder for BOTF Remastered: hero hulls modelled from original three-view SVG drawings.

Run headless (Blender 4.2+ / 5.x), after blender_generate_ships.py:
  blender --background --factory-startup --python blender_blueprint_ships.py -- [--blueprints <dir>] [--out <dir>]
          [--keys ship.kli.*] [--preview]

Every <blueprints>/<faction>/<slug>.svg (see ArtSource/Blueprints/kli/vorcha.svg for the format) becomes
<out>/<faction>/<slug>.fbx, replacing the kit-built hull of the same key, in the same form: nose +X, up +Z, canon length
in metres (taken from <out>/generated-ships.json), the "Mask" colour layer (R windows, G warm, B cool, A accent) and
SOCKET_* empties. A solid part is the intersection of its outlines extruded through the views it is drawn in; the parts
are fused by a voxel remesh, so joints blend like a sculpted hull, then decimated and given smart-projected UVs for the
tiling hull material. Paint volumes colour the finished surface without adding geometry.
"""
import fnmatch
import json
import math
import os
import re
import sys
import xml.etree.ElementTree as ET

import bmesh
import bpy
from mathutils import Matrix, Vector

SVG = "{http://www.w3.org/2000/svg}"
MASKS = {"hull": (0.0, 0.0, 0.0, 0.0), "windows": (1.0, 0.0, 0.0, 0.0), "warm": (0.0, 1.0, 0.0, 0.0),
         "cool": (0.0, 0.0, 1.0, 0.0), "accent": (0.0, 0.0, 0.0, 1.0)}
DEPTH = 5000.0  # prism half-depth, far beyond any 1000-unit drawing


# ----------------------------------------------------------------------------------------------------------------
# SVG outlines -> 2D polygons (local view coordinates)
# ----------------------------------------------------------------------------------------------------------------
def _cubic(p0, p1, p2, p3, n=12):
    out = []
    for i in range(1, n + 1):
        t = i / n
        u = 1 - t
        out.append((u * u * u * p0[0] + 3 * u * u * t * p1[0] + 3 * u * t * t * p2[0] + t * t * t * p3[0],
                    u * u * u * p0[1] + 3 * u * u * t * p1[1] + 3 * u * t * t * p2[1] + t * t * t * p3[1]))
    return out


def parse_path(d):
    """M/L/H/V/C/S/Q/Z, absolute and relative, one closed outline (first subpath)."""
    tokens = re.findall(r"[MmLlHhVvCcSsQqZz]|-?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?", d)
    pts, cur, start, cmd, i, last_ctrl = [], (0.0, 0.0), (0.0, 0.0), None, 0, None

    def num():
        nonlocal i
        v = float(tokens[i])
        i += 1
        return v

    while i < len(tokens):
        if re.match(r"[A-Za-z]", tokens[i]):
            cmd = tokens[i]
            i += 1
            if cmd in "Zz":
                cur = start
                continue
        rel = cmd.islower()
        c = cmd.upper()
        ox, oy = cur if rel else (0.0, 0.0)
        if c == "M":
            cur = (ox + num(), oy + num())
            start = cur
            pts.append(cur)
            cmd = "l" if rel else "L"
        elif c == "L":
            cur = (ox + num(), oy + num())
            pts.append(cur)
        elif c == "H":
            cur = ((cur[0] if rel else 0.0) + num(), cur[1])
            pts.append(cur)
        elif c == "V":
            cur = (cur[0], (cur[1] if rel else 0.0) + num())
            pts.append(cur)
        elif c == "C":
            p1 = (ox + num(), oy + num())
            p2 = (ox + num(), oy + num())
            p3 = (ox + num(), oy + num())
            pts += _cubic(cur, p1, p2, p3)
            last_ctrl, cur = p2, p3
        elif c == "S":
            p1 = (2 * cur[0] - last_ctrl[0], 2 * cur[1] - last_ctrl[1]) if last_ctrl else cur
            p2 = (ox + num(), oy + num())
            p3 = (ox + num(), oy + num())
            pts += _cubic(cur, p1, p2, p3)
            last_ctrl, cur = p2, p3
        elif c == "Q":
            q = (ox + num(), oy + num())
            p3 = (ox + num(), oy + num())
            p1 = (cur[0] + 2 / 3 * (q[0] - cur[0]), cur[1] + 2 / 3 * (q[1] - cur[1]))
            p2 = (p3[0] + 2 / 3 * (q[0] - p3[0]), p3[1] + 2 / 3 * (q[1] - p3[1]))
            pts += _cubic(cur, p1, p2, p3)
            cur = p3
        else:
            raise ValueError("unsupported path command " + cmd)
    clean = []
    for p in pts:
        if not clean or abs(p[0] - clean[-1][0]) + abs(p[1] - clean[-1][1]) > 1e-6:
            clean.append(p)
    if len(clean) > 2 and abs(clean[0][0] - clean[-1][0]) + abs(clean[0][1] - clean[-1][1]) < 1e-6:
        clean.pop()
    return clean


def outline(el):
    tag = el.tag.replace(SVG, "")
    f = lambda k, d=0.0: float(el.get(k, d))
    if tag == "path":
        return parse_path(el.get("d"))
    if tag == "rect":
        x, y, w, h = f("x"), f("y"), f("width"), f("height")
        return [(x, y), (x + w, y), (x + w, y + h), (x, y + h)]
    if tag in ("ellipse", "circle"):
        cx, cy = f("cx"), f("cy")
        rx = f("r") if tag == "circle" else f("rx")
        ry = f("r") if tag == "circle" else f("ry")
        return [(cx + rx * math.cos(a * math.tau / 40), cy + ry * math.sin(a * math.tau / 40)) for a in range(40)]
    if tag == "polygon":
        v = [float(t) for t in re.findall(r"-?\d+\.?\d*", el.get("points"))]
        return list(zip(v[0::2], v[1::2]))
    return None


# View -> (axis the outline is extruded along, function mapping local (u, v) to 3D with the free axis at 0)
VIEWS = {
    "top": ("Z", lambda u, v: (u, -v, 0.0)),
    "side": ("Y", lambda u, v: (u, 0.0, -v)),
    "front": ("X", lambda u, v: (0.0, u, -v)),
}
# 3D point -> the view's 2D coordinates (for paint-volume tests)
PROJECT = {"top": lambda p: (p[0], -p[1]), "side": lambda p: (p[0], -p[2]), "front": lambda p: (p[1], -p[2])}


def read_blueprint(path):
    root = ET.parse(path).getroot()
    meta = json.loads(root.find(SVG + "desc").text)
    parts, paints = {}, []
    for g in root.iter(SVG + "g"):
        view = g.get("id")
        if view not in VIEWS:
            continue
        for el in g:
            poly = outline(el)
            if not poly:
                continue
            mirror = el.get("data-mirror") == "y"
            if el.get("data-part"):
                p = parts.setdefault(el.get("data-part"), {"views": {}, "mirror": False, "mask": "hull"})
                p["views"][view] = poly
                p["mirror"] = p["mirror"] or mirror
                if el.get("data-mask"):
                    p["mask"] = el.get("data-mask")
            elif el.get("data-paint"):
                paints.append({"view": view, "poly": poly, "mask": el.get("data-paint"), "mirror": mirror})
    return meta, parts, paints


# ----------------------------------------------------------------------------------------------------------------
# Geometry
# ----------------------------------------------------------------------------------------------------------------
def prism(name, view, poly):
    axis, to3d = VIEWS[view]
    off = {"X": Vector((DEPTH, 0, 0)), "Y": Vector((0, DEPTH, 0)), "Z": Vector((0, 0, DEPTH))}[axis]
    bm = bmesh.new()
    lo = [bm.verts.new(Vector(to3d(u, v)) - off) for u, v in poly]
    hi = [bm.verts.new(Vector(to3d(u, v)) + off) for u, v in poly]
    n = len(poly)
    bm.faces.new(lo)
    bm.faces.new(list(reversed(hi)))
    for k in range(n):
        bm.faces.new([lo[k], lo[(k + 1) % n], hi[(k + 1) % n], hi[k]])
    bmesh.ops.triangulate(bm, faces=[f for f in bm.faces if len(f.verts) > 4])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    return ob


def apply_all(ob):
    bpy.context.view_layer.objects.active = ob
    for m in list(ob.modifiers):
        bpy.ops.object.modifier_apply(modifier=m.name)


def build_part(name, part):
    views = list(part["views"].items())
    base = prism(name, *views[0])
    cutters = []
    for view, poly in views[1:]:
        c = prism(name + "_" + view, view, poly)
        mod = base.modifiers.new("x" + view, "BOOLEAN")
        mod.operation = "INTERSECT"
        mod.solver = "EXACT"
        mod.object = c
        cutters.append(c)
    apply_all(base)
    for c in cutters:
        bpy.data.objects.remove(c)
    if part["mirror"]:
        mod = base.modifiers.new("mirror", "MIRROR")
        mod.use_axis = (False, True, False)
        apply_all(base)
    return base


def fuse(objs, voxel):
    bpy.ops.object.select_all(action="DESELECT")
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    bpy.ops.object.join()
    hull = objs[0]
    hull.name = "Hull"
    r = hull.modifiers.new("remesh", "REMESH")
    r.mode = "VOXEL"
    r.voxel_size = voxel
    apply_all(hull)
    # Soften the voxel steps without melting the plate edges.
    s = hull.modifiers.new("smooth", "CORRECTIVE_SMOOTH")
    s.iterations = 10
    s.smooth_type = "LENGTH_WEIGHTED"
    s.use_only_smooth = True
    apply_all(hull)
    return hull


def decimate(hull, target_tris):
    """Collapse to the triangle budget, sparing painted areas and their borders: flat hull plates reduce to a few big
    triangles, which would leave no vertices to carry a window row or a grille. Loop colours interpolate on collapse."""
    tris = sum(len(p.vertices) - 2 for p in hull.data.polygons)
    if tris <= target_tris:
        return
    me = hull.data
    col = me.color_attributes["Mask"].data
    painted = set()
    for loop in me.loops:
        if any(col[loop.index].color[k] > 0.01 for k in range(4)):
            painted.add(loop.vertex_index)
    keep = set(painted)
    for e in me.edges:
        a, b = e.vertices
        if (a in painted) != (b in painted):
            keep.update((a, b))
    group = hull.vertex_groups.new(name="Keep")
    group.add(list(keep), 1.0, "REPLACE")
    d = hull.modifiers.new("decimate", "DECIMATE")
    d.ratio = target_tris / tris
    d.vertex_group = "Keep"
    d.invert_vertex_group = True  # the group marks what may collapse; inverted, it marks what to spare
    d.vertex_group_factor = 1.0
    apply_all(hull)
    if "Keep" in hull.vertex_groups:
        hull.vertex_groups.remove(hull.vertex_groups["Keep"])


def inside(poly, p, tol):
    """Point in polygon, or within tol of its outline."""
    x, y = p
    hit = False
    n = len(poly)
    for k in range(n):
        (x1, y1), (x2, y2) = poly[k], poly[(k + 1) % n]
        if (y1 > y) != (y2 > y) and x < (x2 - x1) * (y - y1) / (y2 - y1 + 1e-12) + x1:
            hit = not hit
        dx, dy = x2 - x1, y2 - y1
        t = max(0.0, min(1.0, ((x - x1) * dx + (y - y1) * dy) / (dx * dx + dy * dy + 1e-12)))
        if math.hypot(x - x1 - t * dx, y - y1 - t * dy) <= tol:
            return True
    return hit


def paint(hull, parts, paints, tol):
    """Masks: parts with data-mask paint their own volume; paint volumes of the same mask intersect across views
    (a volume drawn in top and side paints only where both agree); later entries win."""
    volumes = []
    for name, part in parts.items():
        if part["mask"] != "hull":
            volumes.append((part["mask"], [(v, poly, part["mirror"]) for v, poly in part["views"].items()]))
    groups = {}
    for pv in paints:
        groups.setdefault(pv["mask"], []).append(pv)
    for mask, items in groups.items():
        # One volume per mask: a point must fall inside some outline in every view that mask is drawn in.
        by_view = {}
        for pv in items:
            by_view.setdefault(pv["view"], []).append((pv["poly"], pv["mirror"]))
        volumes.insert(0, (mask, [(v, lst, None) for v, lst in by_view.items()]))
    me = hull.data
    col = me.color_attributes.new("Mask", "FLOAT_COLOR", "CORNER")
    vert_mask = []
    for v in me.vertices:
        p = v.co
        chosen = MASKS["hull"]
        for mask, tests in volumes:
            ok = True
            for view, shape, mirror in tests:
                q = PROJECT[view](p)
                qm = PROJECT[view]((p[0], -p[1], p[2]))
                if mirror is None:  # grouped paint outlines: any of them
                    ok = any(inside(poly, q, tol) or (mir and inside(poly, qm, tol)) for poly, mir in shape)
                else:
                    ok = inside(shape, q, tol) or (mirror and inside(shape, qm, tol))
                if not ok:
                    break
            if ok:
                chosen = MASKS[mask]
        vert_mask.append(chosen)
    for loop in me.loops:
        col.data[loop.index].color = vert_mask[loop.vertex_index]
    counts = {n: sum(1 for c in vert_mask if c == MASKS[n]) for n in MASKS}
    print("    mask vertices before decimation: " + ", ".join("%s %d" % kv for kv in counts.items()))


def finish(hull, length):
    bpy.context.view_layer.objects.active = hull
    hull.select_set(True)
    bpy.ops.object.shade_smooth_by_angle(angle=math.radians(38))
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.uv.smart_project(angle_limit=math.radians(55), island_margin=0.002)
    bpy.ops.object.mode_set(mode="OBJECT")
    # Blueprint units -> metres: nose +X, centred on the bounds like the kit ships.
    bb = [Vector(c) for c in hull.bound_box]
    lo = Vector((min(v.x for v in bb), min(v.y for v in bb), min(v.z for v in bb)))
    hi = Vector((max(v.x for v in bb), max(v.y for v in bb), max(v.z for v in bb)))
    k = length / max(hi - lo)
    m = Matrix.Scale(k, 4) @ Matrix.Translation(-(lo + hi) / 2)
    hull.data.transform(m)
    # Texel density of the kit ships: one UV unit per max(4, L/14) m, so the tiling plating keeps its plate size.
    me = hull.data
    uv = me.uv_layers.active.data
    area3d = sum(p.area for p in me.polygons)
    area_uv = 0.0
    for p in me.polygons:
        pts = [uv[i].uv for i in p.loop_indices]
        area_uv += abs(sum(pts[i].x * pts[i - 1].y - pts[i - 1].x * pts[i].y for i in range(len(pts)))) / 2
    unit = max(4.0, length / 14.0)
    s = math.sqrt(area3d / max(area_uv, 1e-9)) / unit
    for d in uv:
        d.uv = d.uv * s
    return m


# ----------------------------------------------------------------------------------------------------------------
# Output (same form as blender_generate_ships.py)
# ----------------------------------------------------------------------------------------------------------------
def export(svg_path, out_dir, lengths, preview_dir):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    meta, parts, paints = read_blueprint(svg_path)
    key = meta["key"]
    length = float(meta.get("length", lengths.get(key, 400.0)))
    objs = [build_part(n, p) for n, p in parts.items()]
    hull = fuse(objs, voxel=float(meta.get("voxel", 2.2)))
    paint(hull, parts, paints, tol=3.0)
    decimate(hull, int(meta.get("triangles", 60000)))
    m = finish(hull, length)
    sockets = []
    for entry in meta.get("sockets", []):
        name, x, y, z = entry[:4]
        mirror = len(entry) > 4 and entry[4] == "mirror"
        for sy in ((y, -y) if mirror and y != 0 else (y,)):
            e = bpy.data.objects.new("SOCKET_" + name, None)
            e.location = m @ Vector((x, sy, z))
            e.empty_display_size = length * 0.01
            bpy.context.scene.collection.objects.link(e)
            e.parent = hull
            sockets.append(name)
    tris = sum(len(p.vertices) - 2 for p in hull.data.polygons)
    fac, slug = key.split(".")[1], key.split(".", 2)[2]
    path = os.path.join(out_dir, fac, slug + ".fbx")
    os.makedirs(os.path.dirname(path), exist_ok=True)
    bpy.ops.export_scene.fbx(filepath=path, use_selection=False, apply_unit_scale=True, global_scale=1.0, axis_forward="-Y", axis_up="Z",
                             object_types={"MESH", "EMPTY"}, mesh_smooth_type="FACE", colors_type="LINEAR", use_mesh_modifiers=True,
                             add_leaf_bones=False, bake_space_transform=False)
    if preview_dir:
        render_preview(hull, length, os.path.join(preview_dir, key + ".blueprint.png"))
    return {"key": key, "file": os.path.relpath(path, out_dir).replace("\\", "/"), "lengthMeters": length, "triangles": tris,
            "archetype": "blueprint", "sockets": sockets}


def render_preview(ob, length, path):
    scene = bpy.context.scene
    mat = bpy.data.materials.new("Preview")
    if mat.node_tree is None:  # Blender 5 materials always have a node tree; use_nodes goes away in 6.0
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
    print("    preview: engine %s, material %s, colour layers %s" % (scene.render.engine, [m.name for m in ob.data.materials], [a.name for a in ob.data.color_attributes]))
    cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
    scene.collection.objects.link(cam)
    cam.location = (length * 0.85, -length * 1.05, length * 0.6)
    direction = Vector((0, 0, 0)) - cam.location
    cam.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()
    cam.data.lens = 50
    cam.data.clip_start = length * 0.01
    cam.data.clip_end = length * 20
    scene.camera = cam
    for rot, energy in (((math.radians(40), 0, math.radians(30)), 4.0), ((math.radians(-60), 0, math.radians(200)), 1.5)):
        light = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
        light.data.energy = energy
        light.rotation_euler = rot
        scene.collection.objects.link(light)
    world = bpy.data.worlds.new("W")
    world.color = (0.02, 0.02, 0.03)
    scene.world = world
    engines = [e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items]
    scene.render.engine = "BLENDER_EEVEE_NEXT" if "BLENDER_EEVEE_NEXT" in engines else "BLENDER_EEVEE"
    print("    preview: engine %s" % scene.render.engine)
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
    bp_dir = os.path.abspath(arg("--blueprints", os.path.join(here, "..", "ArtSource", "Blueprints")))
    out_dir = os.path.abspath(arg("--out", os.path.join(here, "..", "ArtSource", "Generated", "Ships")))
    listing = os.path.join(out_dir, "generated-ships.json")
    ships = {e["key"]: e for e in json.load(open(listing, encoding="utf-8"))["ships"]} if os.path.exists(listing) else {}
    lengths = {k: e["lengthMeters"] for k, e in ships.items()}
    preview_dir = os.path.join(out_dir, "previews") if arg("--preview") else None
    if preview_dir:
        os.makedirs(preview_dir, exist_ok=True)
    pats = arg("--keys").split(",") if isinstance(arg("--keys"), str) else None
    svgs = sorted(os.path.join(dp, f) for dp, _, fs in os.walk(bp_dir) for f in fs if f.endswith(".svg"))
    done = 0
    for svg in svgs:
        fac = os.path.basename(os.path.dirname(svg))
        key = "ship.%s.%s" % (fac, os.path.splitext(os.path.basename(svg))[0])
        if pats and not any(fnmatch.fnmatch(key, p) for p in pats):
            continue
        try:
            r = export(svg, out_dir, lengths, preview_dir)
            ships[r["key"]] = r
            done += 1
            print("OK  %-32s blueprint %6d tris  %d sockets" % (r["key"], r["triangles"], len(r["sockets"])))
        except Exception as e:
            import traceback
            traceback.print_exc()
            print("ERR %s  %s" % (svg, e))
    with open(listing, "w", encoding="utf-8") as f:
        json.dump({"ships": sorted(ships.values(), key=lambda e: e["key"])}, f, indent=1)
    print("blueprints %d/%d" % (done, len(svgs)))


main()
