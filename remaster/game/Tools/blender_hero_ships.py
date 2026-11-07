"""Blender (5.x, headless): turn downloaded base models into game-ready hero ships (private, local only).

    blender -b --factory-startup --python blender_hero_ships.py -- [--keys ship.fed.*,ship.kli.bop] [--preview]
                                                                     [--gltf [--gltf-lods 0]]

Reads ArtSource/HeroShips/hero-ships.json; sources come from LocalAssets/incoming (gitignored). For each ship:
  1. Imports the source (glTF, or OBJ with the axes given in objAxes), drops cameras/lights/empties, joins all parts.
  2. Orients it so the named forward axis becomes the nose (+X) and the named up axis the top (+Z), centres it on
     its bounds and scales it to canon length in metres (the procedural hulls' convention).
  3. Cleans it (merges coincident vertices, drops loose ones) and decimates to maxTriangles when above it.
  4. Upgrades the materials. Slots sharing a texture set are merged. Each keeps its own paint and gets:
     BC (base colour), N (normal: the source's own, else relief derived from the paint's luminance),
     ORM (occlusion from panel cavities, roughness from the paint, metallic from the config or the source's
     metallicRoughness map), E (the source's emissive windows/engines). Untextured models ("plating": fac) are
     unwrapped at the procedural texel density instead and use the faction's plated hull material in Unreal.
  5. Places weapon/engine sockets from the hull shape (nose torpedo, dorsal and ventral beams, aft engine), or the
     config's "sockets" entries ([name, x, y, z] as fractions of the length, nose +X).
  6. Writes LocalAssets/ships/<key>/SM_<slug>.fbx, T_<slug>_<slot>_{BC,N,ORM,E}.png and hero.json (slots, sockets,
     length), plus four preview renders when --preview is given (default C:/Dev/botf-shots/hero).
  7. With --gltf (off by default; the FBX, textures and hero.json are written exactly as without it), also writes
     LocalAssets/ships/<key>/SM_<slug>.glb for the Godot build: +X nose, +Y up, metres; the BC/N/ORM/E bakes embedded
     as glTF PBR materials named like the slots (M00, ...); the plating bake as COLOR_0 ("Col"); the SOCKET_WPN_* /
     SOCKET_ENG_* empties as nodes; reduced copies SM_<slug>_LOD1..3 (--gltf-lods 0 leaves them out). Each built hero
     is merged into LocalAssets/ships/godot-hero-manifest.json (key -> glb, canon length, sockets, LODs and the
     material hints ue_import_hero_ships.py applies; see Tools/hero_gltf_manifest.py).
Tools/ue_import_hero_ships.py brings the result into /Game/Local and the resolved manifest.
"""
import fnmatch
import json
import math
import os
import sys

import bmesh
import bpy
import numpy as np
from mathutils import Matrix, Vector

HERE = os.path.dirname(os.path.abspath(__file__))
GAME = os.path.normpath(os.path.join(HERE, ".."))
CONFIG = os.path.join(GAME, "ArtSource", "HeroShips", "hero-ships.json")
INCOMING = os.path.join(GAME, "LocalAssets", "incoming")
OUT_ROOT = os.path.join(GAME, "LocalAssets", "ships")
AXES = {"+X": Vector((1, 0, 0)), "-X": Vector((-1, 0, 0)), "+Y": Vector((0, 1, 0)), "-Y": Vector((0, -1, 0)),
        "+Z": Vector((0, 0, 1)), "-Z": Vector((0, 0, -1))}
MAX_TEX = 4096


def arg(name, default=None):
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    if name in argv:
        i = argv.index(name)
        return argv[i + 1] if i + 1 < len(argv) and not argv[i + 1].startswith("--") else True
    return default


# ---------------------------------------------------------------------------------------------------------------
# Geometry
# ---------------------------------------------------------------------------------------------------------------

def import_source(ship):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    path = os.path.join(INCOMING, ship["source"])
    ext = os.path.splitext(path)[1].lower()
    if ext in (".gltf", ".glb"):
        bpy.ops.import_scene.gltf(filepath=path)
    elif ext == ".obj":
        fwd, up = ship.get("objAxes", ["NEGATIVE_Z", "Y"])
        bpy.ops.wm.obj_import(filepath=path, forward_axis=fwd, up_axis=up)
    elif ext == ".fbx":
        bpy.ops.import_scene.fbx(filepath=path)
    else:
        raise RuntimeError("unsupported source " + ext)
    meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
    if not meshes:
        raise RuntimeError("no meshes in " + path)
    # Un-parent first: importers put a root empty (axis conversion, unit scale) above the meshes, and deleting it
    # while children still hang off it would drop that transform.
    bpy.ops.object.select_all(action="DESELECT")
    for o in meshes:
        o.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    bpy.ops.object.parent_clear(type="CLEAR_KEEP_TRANSFORM")
    for ob in list(bpy.context.scene.objects):
        if ob.type != "MESH":
            bpy.data.objects.remove(ob, do_unlink=True)
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    # Old exports carry stray helpers (beam guides, lights as meshes) far from the hull: drop parts whose centre lies
    # well outside the spread of all the vertices.
    def verts(o):
        a = np.empty(len(o.data.vertices) * 3, dtype=np.float64)
        o.data.vertices.foreach_get("co", a)
        return a.reshape(-1, 3)
    every = np.concatenate([verts(o) for o in meshes])
    centre = np.median(every, axis=0)
    reach = float(np.percentile(np.linalg.norm(every - centre, axis=1), 95)) * 1.6
    for o in list(meshes):
        v = verts(o)
        if len(meshes) > 1 and (not len(v) or np.linalg.norm((v.min(axis=0) + v.max(axis=0)) / 2 - centre) > reach):
            meshes.remove(o)
            bpy.data.objects.remove(o, do_unlink=True)
    bpy.ops.object.select_all(action="DESELECT")
    for o in meshes:
        o.select_set(True)
    bpy.context.view_layer.objects.active = max(meshes, key=lambda o: len(o.data.vertices))
    if len(meshes) > 1:
        bpy.ops.object.join()
    hull = bpy.context.view_layer.objects.active
    return hull


def bounds(ob):
    co = np.empty(len(ob.data.vertices) * 3, dtype=np.float64)
    ob.data.vertices.foreach_get("co", co)
    co = co.reshape(-1, 3)
    return co, co.min(axis=0), co.max(axis=0)


def orient_and_scale(hull, ship):
    fwd, up = AXES[ship.get("forward", "+X")], AXES[ship.get("up", "+Z")]
    side = up.cross(fwd)
    # Rows are where the source axes go: source forward -> +X, source side -> +Y, source up -> +Z.
    rot = Matrix((fwd, side, up)).to_4x4()
    hull.data.transform(rot)
    _, lo, hi = bounds(hull)
    centre = Vector(((lo + hi) / 2).tolist())
    hull.data.transform(Matrix.Translation(-centre))
    length = float(hi[0] - lo[0])
    scale = ship["lengthMeters"] / max(length, 1e-9)
    hull.data.transform(Matrix.Scale(scale, 4))
    hull.data.update()


def apply_variant(hull, ship):
    """Derives a sister class from its base model (hull oriented, centred, canon length along X, metres).

    "proportions": [sx, sy, sz] rescales the beam and height about the centre; the length stays canon.
    "modules": each copies the faces lying wholly inside the length band "from" [x0, x1] (fractions of the length,
    -0.5 = stern, 0.5 = bow), scales the copy by "scale" about its own centre, moves it by "offset" [x, y, z]
    (fractions of the length) and, with "onTop" / "below", seats it on the hull's dorsal or ventral surface over its
    footprint. "mirror": true adds a second copy reflected across the centreline (outboard pods)."""
    v = ship.get("variant")
    if not v:
        return
    sx, sy, sz = v.get("proportions", [1.0, 1.0, 1.0])
    hull.data.transform(Matrix.Diagonal((sx, sy, sz, 1.0)))
    co, lo, hi = bounds(hull)
    length = float(hi[0] - lo[0])
    for m in v.get("modules", []):
        x0, x1 = (np.array(m["from"], dtype=np.float64) + 0.5) * length + lo[0]
        bm = bmesh.new()
        bm.from_mesh(hull.data)
        bm.verts.ensure_lookup_table()
        faces = [f for f in bm.faces if all(x0 <= vv.co.x <= x1 for vv in f.verts)]
        if not faces:
            bm.free()
            raise RuntimeError("variant module %s selects no faces" % m["from"])
        copies = [1.0, -1.0] if m.get("mirror") else [1.0]
        for side in copies:
            dup = bmesh.ops.duplicate(bm, geom=faces)
            verts = [g for g in dup["geom"] if isinstance(g, bmesh.types.BMVert)]
            pts = np.array([vv.co[:] for vv in verts])
            centre = Vector(((pts.min(axis=0) + pts.max(axis=0)) / 2).tolist())
            k = float(m.get("scale", 1.0))
            ox, oy, oz = [c * length for c in m.get("offset", [0.0, 0.0, 0.0])]
            for vv in verts:
                p = (vv.co - centre) * k
                vv.co = Vector((centre.x + p.x + ox, (centre.y + p.y + oy) * side, centre.z + p.z + oz))
            if m.get("onTop") or m.get("below"):
                # Seat the copy on the hull surface under (or over) its footprint, sunk a fifth of its own height.
                mz = np.array([vv.co.z for vv in verts])
                mx = np.array([vv.co.x for vv in verts])
                band = co[(co[:, 0] >= mx.min()) & (co[:, 0] <= mx.max())]
                if len(band) == 0:
                    band = co
                depth = mz.max() - mz.min()
                shift = (band[:, 2].max() - mz.min() - 0.2 * depth) if m.get("onTop") else (band[:, 2].min() - mz.max() + 0.2 * depth)
                for vv in verts:
                    vv.co.z += shift
            if side < 0:
                # A reflected copy turns inside out; flip it back.
                bmesh.ops.reverse_faces(bm, faces=[g for g in dup["geom"] if isinstance(g, bmesh.types.BMFace)])
        bm.to_mesh(hull.data)
        bm.free()
    # Recentre and hold the canon length (a module may overhang the ends).
    _, lo, hi = bounds(hull)
    hull.data.transform(Matrix.Translation(Vector((-(lo + hi) / 2).tolist())))
    hull.data.transform(Matrix.Scale(ship["lengthMeters"] / max(float(hi[0] - lo[0]), 1e-9), 4))
    hull.data.update()


def clean(hull, max_tris):
    length = float(np.ptp(bounds(hull)[0][:, 0]))
    bm = bmesh.new()
    bm.from_mesh(hull.data)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=length * 2e-6)
    loose = [v for v in bm.verts if not v.link_faces]
    bmesh.ops.delete(bm, geom=loose, context="VERTS")
    bm.to_mesh(hull.data)
    bm.free()
    tris = tri_count(hull)
    if max_tris and tris > max_tris:
        mod = hull.modifiers.new("Decimate", "DECIMATE")
        mod.ratio = max_tris / float(tris)
        mod.use_collapse_triangulate = True
        bpy.context.view_layer.objects.active = hull
        bpy.ops.object.modifier_apply(modifier=mod.name)
    return tris


def tri_count(ob):
    return sum(len(p.vertices) - 2 for p in ob.data.polygons)


# ---------------------------------------------------------------------------------------------------------------
# Materials
# ---------------------------------------------------------------------------------------------------------------

def upstream_image(socket, depth=0):
    """First image texture feeding a socket, and the output channel it came through (R/G/B/A or '')."""
    if not socket.is_linked or depth > 8:
        return None, ""
    link = socket.links[0]
    node = link.from_node
    if node.type == "TEX_IMAGE" and node.image:
        return node.image, ""
    if node.type in ("SEPARATE_COLOR", "SEPRGB", "SEPARATE_RGB"):
        img, _ = upstream_image(node.inputs[0], depth + 1)
        return img, {"Red": "R", "Green": "G", "Blue": "B", "R": "R", "G": "G", "B": "B"}.get(link.from_socket.name, "")
    for inp in node.inputs:
        img, ch = upstream_image(inp, depth + 1)
        if img:
            return img, ch
    return None, ""


def material_sources(mat):
    info = {"bc": None, "n": None, "mr": None, "e": None, "tint": (0.6, 0.6, 0.6), "estrength": 1.0}
    if not mat or not mat.node_tree:
        return info
    bsdf = next((n for n in mat.node_tree.nodes if n.type == "BSDF_PRINCIPLED"), None)
    if not bsdf:
        return info
    info["bc"], _ = upstream_image(bsdf.inputs["Base Color"])
    if not info["bc"]:
        info["tint"] = tuple(bsdf.inputs["Base Color"].default_value[:3])
    info["n"], _ = upstream_image(bsdf.inputs["Normal"])
    rimg, rch = upstream_image(bsdf.inputs["Roughness"])
    if rimg and rch == "G" and "metallic" in rimg.name.lower():
        info["mr"] = rimg
    info["e"], _ = upstream_image(bsdf.inputs["Emission Color"])
    info["estrength"] = float(bsdf.inputs["Emission Strength"].default_value) if info["e"] else 0.0
    return info


def pixels(img):
    w, h = img.size
    a = np.empty(w * h * 4, dtype=np.float32)
    img.pixels.foreach_get(a)
    return a.reshape(h, w, 4)


def blur(a, r):
    """Separable box blur (wrapping), radius r pixels; three passes approximate a gaussian."""
    for _ in range(3):
        for axis in (0, 1):
            c = np.cumsum(np.concatenate([a.take(range(-r - 1, 0), axis=axis), a, a.take(range(0, r), axis=axis)], axis=axis), axis=axis)
            n = a.shape[axis]
            a = (c.take(range(2 * r + 1, 2 * r + 1 + n), axis=axis) - c.take(range(0, n), axis=axis)) / (2 * r + 1)
    return a


def save_png(arr, path, colour):
    h, w = arr.shape[:2]
    rgba = np.ones((h, w, 4), dtype=np.float32)
    rgba[..., :arr.shape[2]] = np.clip(arr, 0, 1)
    img = bpy.data.images.new(os.path.basename(path), w, h, alpha=False)
    img.colorspace_settings.name = "sRGB" if colour else "Non-Color"
    img.pixels.foreach_set(rgba.ravel())
    img.filepath_raw = path
    img.file_format = "PNG"
    img.save()
    bpy.data.images.remove(img)


def build_textures(slot, src, out_dir, stem, metallic, tint=None):
    """Writes the slot's BC/N/ORM/E maps. Returns the file names (None where a map is absent)."""
    files = {"bc": None, "n": None, "orm": None, "e": None}
    if not src["bc"]:
        return files
    bc = pixels(src["bc"])[..., :3]
    h, w = bc.shape[:2]
    step = max(1, int(math.ceil(max(w, h) / MAX_TEX)))
    bc = bc[::step, ::step]
    if tint:
        # A sister class wears its own paint: the base colour multiplied by the variant's tint.
        bc = np.clip(bc * np.array(tint, dtype=np.float32), 0.0, 1.0)
    h, w = bc.shape[:2]
    files["bc"] = f"{stem}_{slot}_BC.png"
    save_png(bc, os.path.join(out_dir, files["bc"]), True)
    lum = bc @ np.array([0.2126, 0.7152, 0.0722], dtype=np.float32)
    k = max(1, w // 512)
    fine = blur(lum, k)
    broad = blur(lum, 12 * k)
    cavity = np.clip((broad - fine) * 4.0, 0, 1)
    if src["n"]:
        n = pixels(src["n"])[..., :3]
        if n.shape[:2] != (h, w):
            ys = (np.arange(h) * n.shape[0] / h).astype(int)
            xs = (np.arange(w) * n.shape[1] / w).astype(int)
            n = n[ys][:, xs]
    else:
        height = fine - broad
        # Kept gentle: a steep relief from paint makes hull highlights sparkle at a distance (seen on the Sovereign).
        strength = 2.5 * (w / 1024.0)
        dx = (np.roll(height, -1, 1) - np.roll(height, 1, 1)) * strength
        dy = (np.roll(height, -1, 0) - np.roll(height, 1, 0)) * strength
        n = np.stack([-dx, dy, np.ones_like(dx)], axis=-1)
        n /= np.linalg.norm(n, axis=-1, keepdims=True)
        n = n * 0.5 + 0.5
    files["n"] = f"{stem}_{slot}_N.png"
    save_png(n, os.path.join(out_dir, files["n"]), False)
    ao = np.clip(1.0 - cavity * 1.6, 0.35, 1.0)
    if src["mr"]:
        mr = pixels(src["mr"])
        ys = (np.arange(h) * mr.shape[0] / h).astype(int)
        xs = (np.arange(w) * mr.shape[1] / w).astype(int)
        mr = mr[ys][:, xs]
        rough, metal = mr[..., 1], mr[..., 2]
    else:
        # Paint drives roughness: darker, dirtier plating is rougher; bright trim and cavities differ.
        rough = np.clip(0.58 - (lum - lum.mean()) * 0.55 + cavity * 0.25, 0.32, 0.92)
        metal = np.full_like(lum, metallic)
    files["orm"] = f"{stem}_{slot}_ORM.png"
    save_png(np.stack([ao, rough, metal], axis=-1), os.path.join(out_dir, files["orm"]), False)
    if src["e"]:
        e = pixels(src["e"])[..., :3]
        if e.max() > 0.02:
            files["e"] = f"{stem}_{slot}_E.png"
            save_png(e, os.path.join(out_dir, files["e"]), True)
            # Share of the map that glows: windows are a few percent, but some sources light whole panels or grilles,
            # which read as flat stickers at full strength. The importer dims the slot by it.
            files["eCoverage"] = round(float((e.max(axis=-1) > 0.15).mean()), 4)
    return files


def merge_slots(hull):
    """One slot per distinct texture set; returns [(slot name, sources)] in slot order."""
    mats = list(hull.data.materials)
    sigs, order, remap = {}, [], {}
    for i, mat in enumerate(mats):
        src = material_sources(mat)
        sig = (src["bc"].name if src["bc"] else "tint%.2f,%.2f,%.2f" % src["tint"],
               src["e"].name if src["e"] else "", src["n"].name if src["n"] else "")
        if sig not in sigs:
            sigs[sig] = len(order)
            order.append(src)
        remap[i] = sigs[sig]
    idx = np.empty(len(hull.data.polygons), dtype=np.int32)
    hull.data.polygons.foreach_get("material_index", idx)
    if len(mats):
        idx = np.vectorize(lambda m: remap.get(int(m), 0))(idx).astype(np.int32)
    hull.data.materials.clear()
    names = []
    for k in range(max(1, len(order))):
        name = "M%02d" % k
        hull.data.materials.append(bpy.data.materials.new(name))
        names.append(name)
    hull.data.polygons.foreach_set("material_index", idx)
    return list(zip(names, order or [material_sources(None)]))


def plating_unwrap(hull, length):
    """Untextured source: one hull slot, UVs at the procedural texel density (1 UV unit = max(4, L/14) m)."""
    bpy.context.view_layer.objects.active = hull
    hull.select_set(True)
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.uv.smart_project(angle_limit=math.radians(55), island_margin=0.002)
    bpy.ops.object.mode_set(mode="OBJECT")
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
    me.materials.clear()
    me.materials.append(bpy.data.materials.new("M00"))
    if "Mask" not in me.color_attributes:
        me.color_attributes.new("Mask", "BYTE_COLOR", "CORNER")
    col = me.color_attributes["Mask"]
    zero = np.zeros(len(col.data) * 4, dtype=np.float32)
    col.data.foreach_set("color", zero)


# Baked plating palette per faction: hull, underside accent, how far the accent climbs, tone mottling.
# Brighter than the shared generated-ship scheme: these hulls are seen close up and lit only by key, rim and fill.
PLATING_BAKE = {
    "kli": ((0.19, 0.25, 0.16), (0.34, 0.11, 0.07), 0.55, 0.20),
    "fed": ((0.56, 0.58, 0.61), (0.42, 0.44, 0.48), 0.30, 0.07),
    "car": ((0.56, 0.46, 0.31), (0.36, 0.27, 0.18), 0.40, 0.12),
    "rom": ((0.24, 0.40, 0.30), (0.44, 0.50, 0.38), 0.35, 0.12),
    "fer": ((0.56, 0.35, 0.19), (0.34, 0.24, 0.14), 0.40, 0.10),
}


def use_gpu():
    """Cycles on the GPU when one is there (OptiX, then CUDA); otherwise the CPU."""
    try:
        prefs = bpy.context.preferences.addons["cycles"].preferences
        for kind in ("OPTIX", "CUDA"):
            try:
                prefs.compute_device_type = kind
                prefs.get_devices()
                devs = [d for d in prefs.devices if d.type == kind]
                if devs:
                    for d in prefs.devices:
                        d.use = d.type == kind
                    bpy.context.scene.cycles.device = "GPU"
                    return kind
            except TypeError:
                continue
    except Exception:
        pass
    bpy.context.scene.cycles.device = "CPU"
    return "CPU"


def bake_plating(hull, fac, length):
    """Untextured source: bakes the plating's large-scale look into the vertex colour "Col" (linear): the faction hull
    colour with an underside accent, tone mottling (blobs about L/7) and plate families (cells about L/18), times
    ambient occlusion from the model's own geometry. The dense hero meshes carry that detail well; the fine panels,
    seams and normals come from the tiling faction plating in M_ShipPlated (ue_import_hero_ships.py), through the
    tiling UVs plating_unwrap made. (A unique texture bake was tried: these hard-surface meshes unwrap into thousands
    of islands and the atlas used 3-13% of the texture.)"""
    hull_c, accent_c, accent_amt, mottle = PLATING_BAKE.get(fac, PLATING_BAKE["fed"])
    me = hull.data
    for name in [a.name for a in me.color_attributes]:
        me.color_attributes.remove(me.color_attributes[name])
    col_attr = me.color_attributes.new("Col", "FLOAT_COLOR", "POINT")
    me.color_attributes.active_color = col_attr

    mat = me.materials[0]
    mat.use_nodes = True
    nt = mat.node_tree
    nt.nodes.clear()
    N = nt.nodes.new
    L = nt.links.new
    out = N("ShaderNodeOutputMaterial")
    emit = N("ShaderNodeEmission")
    L(emit.outputs["Emission"], out.inputs["Surface"])

    def math_node(op, a=None, b=None, c=None):
        m = N("ShaderNodeMath")
        m.operation = op
        for i, v in enumerate((a, b, c)):
            if v is None:
                continue
            if isinstance(v, (int, float)):
                m.inputs[i].default_value = v
            else:
                L(v, m.inputs[i])
        return m.outputs[0]
    obj = N("ShaderNodeTexCoord")
    noise = N("ShaderNodeTexNoise")
    noise.inputs["Scale"].default_value = 7.0 / length
    noise.inputs["Detail"].default_value = 3.0
    L(obj.outputs["Object"], noise.inputs["Vector"])
    vor = N("ShaderNodeTexVoronoi")
    vor.inputs["Scale"].default_value = 18.0 / length
    L(obj.outputs["Object"], vor.inputs["Vector"])
    mot = math_node("MULTIPLY_ADD", math_node("SUBTRACT", noise.outputs["Fac"], 0.5), 2.0 * mottle, 1.0)
    fam = math_node("MULTIPLY_ADD", math_node("SUBTRACT", vor.outputs["Distance"], 0.5), 0.5 * mottle, 1.0)
    geo = N("ShaderNodeNewGeometry")
    nz = N("ShaderNodeSeparateXYZ")
    L(geo.outputs["Normal"], nz.inputs["Vector"])
    acc_f = N("ShaderNodeMapRange")
    acc_f.inputs["From Min"].default_value = 0.15
    acc_f.inputs["From Max"].default_value = 0.75
    acc_f.inputs["To Max"].default_value = accent_amt
    L(math_node("SUBTRACT", 0.0, nz.outputs["Z"]), acc_f.inputs["Value"])
    def rgba(sockets, name):
        # ShaderNodeMix carries float, vector and colour sockets under the same names; the colour one is wanted.
        return next(x for x in sockets if x.name == name and x.type == "RGBA")
    mix = N("ShaderNodeMix")
    mix.data_type = "RGBA"
    rgba(mix.inputs, "A").default_value = (*hull_c, 1)
    rgba(mix.inputs, "B").default_value = (*accent_c, 1)
    L(acc_f.outputs["Result"], mix.inputs["Factor"])
    ao = N("ShaderNodeAmbientOcclusion")
    ao.samples = 16
    ao.inputs["Distance"].default_value = length * 0.04
    ao_t = math_node("MULTIPLY_ADD", math_node("POWER", ao.outputs["AO"], 1.4), 0.72, 0.28)
    shade = math_node("MULTIPLY", math_node("MULTIPLY", mot, fam), ao_t)
    col = N("ShaderNodeMix")
    col.data_type = "RGBA"
    col.blend_type = "MULTIPLY"
    col.inputs["Factor"].default_value = 1.0
    L(rgba(mix.outputs, "Result"), rgba(col.inputs, "A"))
    L(shade, rgba(col.inputs, "B"))
    L(rgba(col.outputs, "Result"), emit.inputs["Color"])

    scene = bpy.context.scene
    scene.render.engine = "CYCLES"
    device = use_gpu()
    scene.cycles.samples = 32
    bpy.ops.object.select_all(action="DESELECT")
    hull.select_set(True)
    bpy.context.view_layer.objects.active = hull
    bpy.ops.object.bake(type="EMIT", target="VERTEX_COLORS")
    # The FBX path Unreal reads (as for the generated ships' "Mask") is a byte, per-corner colour layer.
    bpy.ops.object.mode_set(mode="OBJECT")
    bpy.ops.geometry.color_attribute_convert(domain="CORNER", data_type="BYTE_COLOR")
    me.color_attributes.active_color = me.color_attributes["Col"]
    me.color_attributes.render_color_index = me.color_attributes.active_color_index
    me.materials.clear()
    me.materials.append(bpy.data.materials.new("M00"))
    cols = me.color_attributes["Col"].data
    step = max(1, len(cols) // 4000)
    picks = [cols[i].color for i in range(0, len(cols), step)]
    mean = [sum(c[i] for c in picks) / len(picks) for i in range(3)]
    print("HERO vertex bake %s %d verts on %s, mean %s" % (hull.name, len(me.vertices), device, [round(v, 3) for v in mean]))

# ---------------------------------------------------------------------------------------------------------------
# Sockets, preview, export
# ---------------------------------------------------------------------------------------------------------------

def auto_sockets(hull, length):
    co, lo, hi = bounds(hull)
    half_w = max(hi[1] - lo[1], 1e-6) * 0.5
    core = co[np.abs(co[:, 1]) < half_w * 0.15]
    core = core if len(core) else co
    nose = core[core[:, 0].argmax()]
    front = co[co[:, 0] > lo[0] + (hi[0] - lo[0]) * 0.45]
    top = front[front[:, 2].argmax()]
    bottom = front[front[:, 2].argmin()]
    aft = core[core[:, 0] < lo[0] + (hi[0] - lo[0]) * 0.1]
    eng_z = float(aft[:, 2].mean()) if len(aft) else 0.0
    return [("WPN_Torp_01", (nose[0] - length * 0.01, 0.0, nose[2])),
            ("WPN_Beam_01", (top[0], 0.0, top[2])),
            ("WPN_Beam_02", (bottom[0], 0.0, bottom[2])),
            ("ENG_01", (lo[0], 0.0, eng_z))]


def add_sockets(hull, ship, length):
    sockets = auto_sockets(hull, length)
    if ship.get("sockets"):
        sockets = [(s[0], (s[1] * length, s[2] * length, s[3] * length)) for s in ship["sockets"]]
    for name, at in sockets:
        e = bpy.data.objects.new("SOCKET_" + name, None)
        e.location = Vector(at)
        e.empty_display_size = length * 0.01
        bpy.context.scene.collection.objects.link(e)
        e.parent = hull
    return [s[0] for s in sockets]


PLATING_TINT = {"fed": (0.46, 0.48, 0.52), "kli": (0.17, 0.22, 0.17), "rom": (0.20, 0.36, 0.26), "car": (0.50, 0.41, 0.27),
                "fer": (0.52, 0.32, 0.17)}  # HullColor of MI_Hull_<fac> (ue_import_generated_ships.py SCHEMES)


def preview_material(name, files, out_dir, tint):
    mat = bpy.data.materials.new("P_" + name)
    if mat.node_tree is None:
        mat.use_nodes = True
    nt = mat.node_tree
    bsdf = nt.nodes.get("Principled BSDF")

    def tex(fn, colour, folder=out_dir):
        node = nt.nodes.new("ShaderNodeTexImage")
        node.image = bpy.data.images.load(os.path.join(folder, fn))
        node.image.colorspace_settings.name = "sRGB" if colour else "Non-Color"
        return node
    if files.get("plating"):
        # Mirrors M_ShipHull: panel tone (R) around the hull colour, seams (G) darkened, UVs x PanelScale 0.35.
        fac = files["plating"]
        gen = os.path.join(GAME, "ArtSource", "Generated", "Ships")
        own = os.path.exists(os.path.join(gen, "T_HullPanels_%s.png" % fac))
        uv = nt.nodes.new("ShaderNodeTexCoord")
        sc = nt.nodes.new("ShaderNodeVectorMath")
        sc.operation = "SCALE"
        sc.inputs["Scale"].default_value = 0.35
        nt.links.new(uv.outputs["UV"], sc.inputs[0])
        panels = tex("T_HullPanels_%s.png" % fac if own else "T_HullPanels.png", False, gen)
        normal = tex("T_HullNormal_%s.png" % fac if own else "T_HullNormal.png", False, gen)
        for node in (panels, normal):
            nt.links.new(sc.outputs["Vector"], node.inputs["Vector"])
        sep = nt.nodes.new("ShaderNodeSeparateColor")
        nt.links.new(panels.outputs["Color"], sep.inputs["Color"])
        tone = nt.nodes.new("ShaderNodeMath")
        tone.operation = "MULTIPLY_ADD"
        nt.links.new(sep.outputs["Red"], tone.inputs[0])
        tone.inputs[1].default_value, tone.inputs[2].default_value = 0.8, 0.6
        seam = nt.nodes.new("ShaderNodeMath")
        seam.operation = "MULTIPLY_ADD"
        nt.links.new(sep.outputs["Green"], seam.inputs[0])
        seam.inputs[1].default_value, seam.inputs[2].default_value = -0.6, 1.0
        shade = nt.nodes.new("ShaderNodeMath")
        shade.operation = "MULTIPLY"
        nt.links.new(tone.outputs[0], shade.inputs[0])
        nt.links.new(seam.outputs[0], shade.inputs[1])
        col = nt.nodes.new("ShaderNodeMix")
        col.data_type = "RGBA"
        col.blend_type = "MULTIPLY"
        col.inputs["Factor"].default_value = 1.0
        col.inputs["A"].default_value = (*PLATING_TINT.get(fac, (0.45, 0.45, 0.48)), 1)
        if files.get("vertexBake"):
            # Mirrors M_ShipPlated: the baked vertex colour instead of the flat faction tint.
            vcol = nt.nodes.new("ShaderNodeVertexColor")
            vcol.layer_name = "Col"
            nt.links.new(vcol.outputs["Color"], col.inputs["A"])
            tone.inputs[1].default_value, tone.inputs[2].default_value = 0.7, 0.65
            seam.inputs[1].default_value = -0.55
        nt.links.new(shade.outputs[0], col.inputs["B"])
        nt.links.new(col.outputs["Result"], bsdf.inputs["Base Color"])
        nm = nt.nodes.new("ShaderNodeNormalMap")
        nt.links.new(normal.outputs["Color"], nm.inputs["Color"])
        nt.links.new(nm.outputs["Normal"], bsdf.inputs["Normal"])
        nt.links.new(panels.outputs["Alpha"], bsdf.inputs["Roughness"])
        bsdf.inputs["Metallic"].default_value = 0.15 if files.get("vertexBake") else 0.5
        return mat
    if files["bc"]:
        nt.links.new(tex(files["bc"], True).outputs["Color"], bsdf.inputs["Base Color"])
    else:
        bsdf.inputs["Base Color"].default_value = (*tint, 1)
    if files["n"]:
        nm = nt.nodes.new("ShaderNodeNormalMap")
        nt.links.new(tex(files["n"], False).outputs["Color"], nm.inputs["Color"])
        nt.links.new(nm.outputs["Normal"], bsdf.inputs["Normal"])
    if files["orm"]:
        sep = nt.nodes.new("ShaderNodeSeparateColor")
        nt.links.new(tex(files["orm"], False).outputs["Color"], sep.inputs["Color"])
        nt.links.new(sep.outputs["Green"], bsdf.inputs["Roughness"])
        nt.links.new(sep.outputs["Blue"], bsdf.inputs["Metallic"])
    else:
        bsdf.inputs["Metallic"].default_value = 0.5
        bsdf.inputs["Roughness"].default_value = 0.45
    if files["e"]:
        nt.links.new(tex(files["e"], True).outputs["Color"], bsdf.inputs["Emission Color"])
        bsdf.inputs["Emission Strength"].default_value = 4.0
    return mat


def render_previews(hull, length, key, preview_dir, slot_files, out_dir):
    scene = bpy.context.scene
    keep = [m.name for m in hull.data.materials]
    for i, (name, files, tint) in enumerate(slot_files):
        hull.data.materials[i] = preview_material(name, files, out_dir, tint)
    world = bpy.data.worlds.new("W")
    world.color = (0.012, 0.014, 0.02)
    scene.world = world
    for rot, energy, colour in (((45, 0, 35), 4.5, (1.0, 0.96, 0.9)), ((-50, 0, 210), 1.2, (0.55, 0.65, 1.0)),
                                ((160, 0, 100), 0.8, (1.0, 0.8, 0.6))):
        light = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
        light.data.energy = energy
        light.data.color = colour
        light.rotation_euler = [math.radians(a) for a in rot]
        scene.collection.objects.link(light)
    cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
    scene.collection.objects.link(cam)
    scene.camera = cam
    engines = [x.identifier for x in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items]
    scene.render.engine = "BLENDER_EEVEE_NEXT" if "BLENDER_EEVEE_NEXT" in engines else "BLENDER_EEVEE"
    scene.render.resolution_x, scene.render.resolution_y = 1600, 900
    scene.view_settings.view_transform = "AgX"
    co, lo, hi = bounds(hull)
    radius = float(np.linalg.norm(co, axis=1).max())
    for tag, d, ortho in (("", (0.9, -1.05, 0.55), False), ("-side", (0, -1, 0), True), ("-top", (0, -0.0001, 1), True),
                          ("-front", (1, 0, 0), True)):
        cam.data.type = "ORTHO" if ortho else "PERSP"
        cam.data.ortho_scale = length * 1.15
        cam.data.lens = 55
        cam.data.clip_start = length * 0.01
        cam.data.clip_end = length * 30
        # Frame the bounding sphere in the vertical field of view (16:9, 55 mm on a 36 mm sensor).
        fov_v = 2 * math.atan(36 * 9 / 16 / 2 / 55)
        cam.location = Vector(d).normalized() * (length * 3 if ortho else radius / math.sin(fov_v / 2) * 1.05)
        cam.rotation_euler = (-cam.location).to_track_quat("-Z", "Y" if tag == "-top" else "Z").to_euler()
        if tag == "-top":  # nose to the right, port side up, like the side view
            cam.rotation_euler = (Matrix.Rotation(math.pi, 4, "Z") @ cam.rotation_euler.to_matrix().to_4x4()).to_euler()
            cam.location = Vector((0, 0, length * 3))
        scene.render.filepath = os.path.join(preview_dir, key + ".hero%s.png" % tag)
        bpy.ops.render.render(write_still=True)
    for i, name in enumerate(keep):
        hull.data.materials[i] = bpy.data.materials[name]


# ---------------------------------------------------------------------------------------------------------------
# glTF for Godot (--gltf)
# ---------------------------------------------------------------------------------------------------------------

def gltf_output_node(nt):
    """The "glTF Material Output" group the glTF exporter reads ambient occlusion from."""
    group = bpy.data.node_groups.get("glTF Material Output")
    if group is None:
        group = bpy.data.node_groups.new("glTF Material Output", "ShaderNodeTree")
        if hasattr(group, "interface"):
            group.interface.new_socket("Occlusion", in_out="INPUT", socket_type="NodeSocketFloat")
        else:
            group.inputs.new("NodeSocketFloat", "Occlusion")
    node = nt.nodes.new("ShaderNodeGroup")
    node.node_tree = group
    return node


def gltf_material(slot, out_dir):
    """A glTF-exportable Principled material for a hero.json slot, named like the slot. Scales and floors stay out of
    it (the Godot hull material applies them from the manifest hints), so the textures export as baked."""
    mat = bpy.data.materials.new(slot["name"])
    mat.use_nodes = True
    nt = mat.node_tree
    bsdf = next(n for n in nt.nodes if n.type == "BSDF_PRINCIPLED")

    def tex(fn, colour):
        node = nt.nodes.new("ShaderNodeTexImage")
        node.image = bpy.data.images.load(os.path.join(out_dir, fn))
        node.image.colorspace_settings.name = "sRGB" if colour else "Non-Color"
        return node
    if slot.get("plating"):
        if slot.get("vertexBake"):
            vcol = nt.nodes.new("ShaderNodeVertexColor")
            vcol.layer_name = "Col"
            nt.links.new(vcol.outputs["Color"], bsdf.inputs["Base Color"])
        else:
            bsdf.inputs["Base Color"].default_value = (*PLATING_TINT.get(slot["plating"], (0.45, 0.45, 0.48)), 1)
        bsdf.inputs["Roughness"].default_value = 0.6
        bsdf.inputs["Metallic"].default_value = 0.35 * slot.get("metallic", 0.45)
        return mat
    if slot.get("bc"):
        nt.links.new(tex(slot["bc"], True).outputs["Color"], bsdf.inputs["Base Color"])
    else:
        bsdf.inputs["Base Color"].default_value = (*(slot.get("tint") or (0.6, 0.6, 0.6)), 1)
    if slot.get("n"):
        nm = nt.nodes.new("ShaderNodeNormalMap")
        nt.links.new(tex(slot["n"], False).outputs["Color"], nm.inputs["Color"])
        nt.links.new(nm.outputs["Normal"], bsdf.inputs["Normal"])
    if slot.get("orm"):
        orm = tex(slot["orm"], False)
        sep = nt.nodes.new("ShaderNodeSeparateColor")
        nt.links.new(orm.outputs["Color"], sep.inputs["Color"])
        nt.links.new(sep.outputs["Green"], bsdf.inputs["Roughness"])
        nt.links.new(sep.outputs["Blue"], bsdf.inputs["Metallic"])
        nt.links.new(sep.outputs["Red"], gltf_output_node(nt).inputs["Occlusion"])
    else:
        bsdf.inputs["Metallic"].default_value = slot.get("metallic", 0.5)
        bsdf.inputs["Roughness"].default_value = 1.0
    if slot.get("e"):
        nt.links.new(tex(slot["e"], True).outputs["Color"], bsdf.inputs["Emission Color"])
        bsdf.inputs["Emission Strength"].default_value = 1.0
    return mat


def gltf_export_kwargs(**wanted):
    """Only the options this Blender's glTF exporter has (their names moved between 3.x, 4.x and 5.x)."""
    try:
        props = {p.identifier for p in bpy.ops.export_scene.gltf.get_rna_type().properties}
    except Exception:
        return wanted
    return {k: v for k, v in wanted.items() if k in props}


def export_gltf(hull, ship, report, slots, out_dir, slug, lods_on):
    """Writes SM_<slug>.glb next to the FBX (see the module docstring) and returns its godot-hero-manifest.json entry.
    Runs after the FBX, hero.json and previews are written, so those stay as they are without --gltf."""
    import shutil
    sys.path.insert(0, HERE)
    import hero_gltf_manifest as hgm
    # The glTF materials replace the bare slot materials; their names (M00, ...) are what the manifest hints key on.
    for i, slot in enumerate(slots):
        if i >= len(hull.data.materials):
            break
        old = hull.data.materials[i]
        if old is not None:
            old.name = old.name + "_fbx"
        hull.data.materials[i] = gltf_material(slot, out_dir)
    if "Col" in hull.data.color_attributes:
        hull.data.color_attributes.active_color = hull.data.color_attributes["Col"]
    steps = hgm.LOD_STEPS if lods_on else hgm.LOD_STEPS[:1]
    plan = hgm.lod_plan(slug, tri_count(hull), steps=steps)
    extra = []
    for lod in plan[1:]:
        dup = hull.copy()
        dup.data = hull.data.copy()
        dup.name = dup.data.name = lod["node"]
        bpy.context.scene.collection.objects.link(dup)
        mod = dup.modifiers.new("Decimate", "DECIMATE")
        mod.ratio = lod["ratio"]
        mod.use_collapse_triangulate = True
        extra.append(dup)
    # The socket empties stay children of LOD0 only: hull.copy() does not copy children.
    glb = os.path.join(out_dir, "SM_%s.glb" % slug)
    bpy.ops.export_scene.gltf(**gltf_export_kwargs(
        filepath=glb, export_format="GLB", use_selection=False, export_yup=True, export_apply=True,
        export_texcoords=True, export_normals=True, export_tangents=True, export_materials="EXPORT",
        export_image_format="AUTO", export_vertex_color="ACTIVE", export_all_vertex_colors=False,
        export_colors=True, export_attributes=False, export_extras=False, export_cameras=False,
        export_lights=False, export_animations=False, export_skins=False, export_morph=False))
    for dup in extra:
        mesh = dup.data
        bpy.data.objects.remove(dup, do_unlink=True)
        bpy.data.meshes.remove(mesh)
    # The faction plating, when the generated-ship pipeline has made it, travels with the glb for the plated material.
    plating_files = {}
    fac = next((s["plating"] for s in slots if s.get("plating")), None)
    if fac:
        gen = os.path.join(GAME, "ArtSource", "Generated", "Ships")
        for kind, stem in (("panels", "T_HullPanels"), ("normal", "T_HullNormal")):
            for name in ("%s_%s.png" % (stem, fac), stem + ".png"):
                if os.path.exists(os.path.join(gen, name)):
                    os.makedirs(os.path.join(OUT_ROOT, "plating"), exist_ok=True)
                    shutil.copy2(os.path.join(gen, name), os.path.join(OUT_ROOT, "plating", name))
                    plating_files[kind] = "plating/" + name
                    break
    rel = os.path.relpath(glb, OUT_ROOT)
    print("HERO glTF %s %s (%d LODs)" % (report["key"], rel, len(plan)))
    return hgm.hero_entry(report, rel, plan, ship.get("bcTint"), plating_files)


def build(ship, defaults, preview_dir, gltf=False, gltf_lods=True):
    key = ship["key"]
    fac, slug = key.split(".")[1], key.split(".", 2)[2]
    stem = "T_%s_%s" % (fac, slug)
    out_dir = os.path.join(OUT_ROOT, key)
    os.makedirs(out_dir, exist_ok=True)
    hull = import_source(ship)
    orient_and_scale(hull, ship)
    apply_variant(hull, ship)
    tris_in = clean(hull, ship.get("maxTriangles", defaults["maxTriangles"]))
    length = float(ship["lengthMeters"])
    metallic = float(ship.get("metallic", defaults["metallic"]))
    slots = []
    if ship.get("plating") and ship.get("bakePlating", True):
        # Untextured model: bake its own plated base colour, then treat it like a textured model.
        plating_unwrap(hull, length)
        bake_plating(hull, ship["plating"], length)
        slots.append({"name": "M00", "plating": ship["plating"], "vertexBake": True, "bc": None, "n": None, "orm": None,
                      "e": None, "tint": None, "metallic": metallic, "emissive": 0.0})
        slot_files = [("M00", {"bc": None, "n": None, "orm": None, "e": None, "plating": ship["plating"], "vertexBake": True},
                       (1.0, 1.0, 1.0))]
    elif ship.get("plating"):
        plating_unwrap(hull, length)
        slots.append({"name": "M00", "plating": ship["plating"], "bc": None, "n": None, "orm": None, "e": None,
                      "tint": None, "emissive": 0.0})
        slot_files = [("M00", {"bc": None, "n": None, "orm": None, "e": None, "plating": ship["plating"]}, (0.45, 0.47, 0.5))]
    else:
        slot_files = []
        for name, src in merge_slots(hull):
            files = build_textures(name, src, out_dir, stem, metallic, ship.get("bcTint"))
            slots.append({"name": name, **files, "tint": None if files["bc"] else list(src["tint"]),
                          "metallic": metallic, "emissive": src["estrength"]})
            slot_files.append((name, files, src["tint"]))
    bpy.ops.object.select_all(action="DESELECT")
    hull.select_set(True)
    bpy.context.view_layer.objects.active = hull
    bpy.ops.object.shade_smooth_by_angle(angle=math.radians(40), keep_sharp_edges=True)
    hull.name = hull.data.name = "SM_%s" % slug
    sockets = add_sockets(hull, ship, length)
    fbx = os.path.join(out_dir, "SM_%s.fbx" % slug)
    bpy.ops.export_scene.fbx(filepath=fbx, use_selection=False, apply_unit_scale=True, global_scale=1.0, axis_forward="-Y",
                             axis_up="Z", object_types={"MESH", "EMPTY"}, mesh_smooth_type="FACE", colors_type="LINEAR",
                             use_mesh_modifiers=True, add_leaf_bones=False, bake_space_transform=False, path_mode="STRIP")
    report = {"key": key, "faction": fac, "file": os.path.basename(fbx), "lengthMeters": length, "trianglesIn": tris_in,
              "triangles": tri_count(hull), "sockets": sockets, "slots": slots, "source": ship["source"]}
    with open(os.path.join(out_dir, "hero.json"), "w", encoding="utf-8") as f:
        json.dump(report, f, indent=1)
    if preview_dir:
        render_previews(hull, length, key, preview_dir, slot_files, out_dir)
    if gltf:
        return dict(report, gltf=export_gltf(hull, ship, report, slots, out_dir, slug, gltf_lods))
    return report


def main():
    config = json.load(open(CONFIG, encoding="utf-8-sig"))
    defaults = config.get("defaults", {})
    pats = arg("--keys").split(",") if isinstance(arg("--keys"), str) else None
    preview = arg("--preview")
    preview_dir = (preview if isinstance(preview, str) else r"C:\Dev\botf-shots\hero") if preview else None
    if preview_dir:
        os.makedirs(preview_dir, exist_ok=True)
    gltf = bool(arg("--gltf"))
    gltf_lods = str(arg("--gltf-lods", "1")) not in ("0", "false", "no")
    gltf_entries = {}
    done = total = 0
    for ship in config["ships"]:
        if pats and not any(fnmatch.fnmatch(ship["key"], p) for p in pats):
            continue
        total += 1
        try:
            r = build(ship, defaults, preview_dir, gltf, gltf_lods)
            if gltf:
                gltf_entries[r["key"]] = r["gltf"]
            done += 1
            print("HERO OK  %-22s %7d -> %6d tris  %d slots  %d sockets" % (r["key"], r["trianglesIn"], r["triangles"],
                                                                         len(r["slots"]), len(r["sockets"])))
        except Exception as e:
            import traceback
            traceback.print_exc()
            print("HERO ERR %s  %s" % (ship["key"], e))
    print("hero ships %d/%d" % (done, total))
    if gltf:
        sys.path.insert(0, HERE)
        import hero_gltf_manifest as hgm
        path = os.path.join(OUT_ROOT, hgm.MANIFEST_NAME)
        data = hgm.merge_manifest(path, gltf_entries)
        print("HERO glTF manifest %s: %d new, %d heroes" % (path, len(gltf_entries), len(data["heroes"])))


main()
