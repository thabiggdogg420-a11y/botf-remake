"""Blender entry for build_art2_ships.py (L9): kitbashes the hulls the library lacks and builds the 30 m Peregrine.

    blender -b --factory-startup -noaudio --python blender_art2_entry.py -- [--keys ship.fed.sydney,...]

Reuses game/Tools/blender_hero_ships.py unmodified (its functions: gltf_material, export_gltf, add sockets, plating_unwrap,
the LOD plan and manifest hints), redirected to the main checkout's LocalAssets, so a kitbash comes out exactly like the
wave-1 heroes: SM_<slug>.glb with LODs and SOCKET_* empties, hero.json, textures, and a godot-hero-manifest.json entry.
"""
import copy
import fnmatch
import json
import math
import os
import shutil
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REMASTER = os.path.abspath(os.path.join(HERE, '..', '..'))
TOOLS = os.path.join(REMASTER, 'game', 'Tools')
LOCAL = os.environ['BOTF_LOCAL_ASSETS']
path = os.path.join(TOOLS, 'blender_hero_ships.py')
src = open(path, encoding='utf-8').read()
subs = [('OUT_ROOT = os.path.join(GAME, "LocalAssets", "ships")', 'OUT_ROOT = %r' % os.path.join(LOCAL, 'ships')),
        ('INCOMING = os.path.join(GAME, "LocalAssets", "incoming")', 'INCOMING = %r' % os.path.join(LOCAL, 'incoming'))]
for a, b in subs:
    assert a in src, 'blender_hero_ships.py changed: ' + a
    src = src.replace(a, b)
assert src.rstrip().endswith('main()')
src = src.rstrip()[:-len('main()')]
G = {'__name__': 'hero_lib', '__file__': path}
sys.path.insert(0, TOOLS)
exec(compile(src, path, 'exec'), G)
sys.path.insert(0, HERE)

import bpy  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Vector  # noqa: E402

import kb_core  # noqa: E402
import kb_recipes  # noqa: E402

SHIPS = os.path.join(LOCAL, 'ships')


def arg(name):
    a = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    return a[a.index(name) + 1] if name in a and a.index(name) + 1 < len(a) else None


def import_base(base_key):
    d = os.path.join(SHIPS, base_key)
    glb = [f for f in os.listdir(d) if f.endswith('.glb')][0]
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=os.path.join(d, glb))
    scene = bpy.context.scene
    meshes = [o for o in scene.objects if o.type == 'MESH' and '_LOD' not in o.name]
    sockets = [(o.name[len('SOCKET_'):], o.matrix_world.translation.copy()) for o in scene.objects if o.type == 'EMPTY' and o.name.startswith('SOCKET_')]
    hull = meshes[0]
    if len(meshes) > 1:
        bpy.ops.object.select_all(action='DESELECT')
        for m in meshes:
            m.select_set(True)
        bpy.context.view_layer.objects.active = hull
        bpy.ops.object.join()
    # bake the world transform into the mesh and drop everything else
    hull.data.transform(hull.matrix_world)
    hull.parent = None
    hull.matrix_world = G['Matrix'].Identity(4)
    for o in list(scene.objects):
        if o is not hull:
            bpy.data.objects.remove(o, do_unlink=True)
    return hull, sockets, json.load(open(os.path.join(d, 'hero.json'), encoding='utf-8-sig'))


def main_slot(hull):
    cnt = {}
    for p in hull.data.polygons:
        cnt[p.material_index] = cnt.get(p.material_index, 0) + p.area
    return max(cnt, key=cnt.get)


def slot_image(mat):
    for n in mat.node_tree.nodes:
        if n.type == 'TEX_IMAGE' and n.image is not None and '_BC' in n.image.name.upper():
            return n.image
    for n in mat.node_tree.nodes:
        if n.type == 'TEX_IMAGE' and n.image is not None:
            return n.image
    return None


def build_kitbash(key, recipe):
    fac, slug = key.split('.')[1], key.split('.', 2)[2]
    base_key = recipe['base']
    hull, sockets, base_hero = import_base(base_key)
    mats = list(hull.data.materials)
    order = [s['name'] for s in base_hero['slots']]
    # material slot order must match the hero.json slot order (M00, M01, ...)
    by_name = {m.name.split('.')[0]: i for i, m in enumerate(mats)}
    ms = main_slot(hull)
    img = slot_image(mats[ms]) if mats else None
    plain = kb_core.plain_patch(img) if img is not None else (0.5, 0.5)
    kit = kb_core.Kit(hull, ms, plain)
    lo0, ext0 = kit.lo.copy(), kit.ext.copy()
    recipe['fn'](kit)
    kit.finish()
    # sockets follow the hull: remap by fractional position in the base bounds to the new bounds is not possible after
    # recipe edits, so the base positions are kept, rescaled with the final length like the geometry
    co = np.array([v.co[:] for v in hull.data.vertices])
    lo, hi = co.min(0), co.max(0)
    length = float(recipe['length'])
    s = length / max((hi - lo).max() if recipe.get('scale_max') else hi[0] - lo[0], 1e-6)
    ctr = (lo + hi) / 2
    for v in hull.data.vertices:
        v.co = (v.co - Vector(ctr)) * s
    hull.data.update()
    G['bpy'].context.view_layer.update()
    hull.name = hull.data.name = 'SM_%s' % slug
    G['bpy'].ops.object.select_all(action='DESELECT')
    hull.select_set(True)
    G['bpy'].context.view_layer.objects.active = hull
    G['bpy'].ops.object.shade_smooth_by_angle(angle=math.radians(40), keep_sharp_edges=True)
    out_dir = os.path.join(SHIPS, key)
    os.makedirs(out_dir, exist_ok=True)
    base_fac, base_slug = base_key.split('.')[1], base_key.split('.', 2)[2]
    old_stem, new_stem = 'T_%s_%s' % (base_fac, base_slug), 'T_%s_%s' % (fac, slug)
    slots = copy.deepcopy(base_hero['slots'])
    for sl in slots:
        for k in ('bc', 'n', 'orm', 'e'):
            if sl.get(k):
                nn = sl[k].replace(old_stem, new_stem)
                shutil.copy2(os.path.join(SHIPS, base_key, sl[k]), os.path.join(out_dir, nn))
                sl[k] = nn
    # hull materials in slot order
    ordered = []
    for nm in order:
        i = by_name.get(nm)
        ordered.append(mats[i] if i is not None else None)
    hull.data.materials.clear()
    for m in ordered:
        hull.data.materials.append(m)
    # remap material indexes (they follow `mats` order already when names are M00.. in order)
    # sockets: re-add as empties at the base positions scaled by the same factor about the same centre
    sock_names = []
    for name, loc in sockets:
        e = G['bpy'].data.objects.new('SOCKET_' + name, None)
        fr = (np.array(loc[:]) - lo0) / ext0
        e.location = Vector(((lo - ctr) * s) + fr * ((hi - lo) * s))
        e.empty_display_size = length * 0.01
        G['bpy'].context.scene.collection.objects.link(e)
        e.parent = hull
        sock_names.append(name)
    report = {'key': key, 'faction': fac, 'file': 'SM_%s.glb' % slug, 'lengthMeters': length, 'trianglesIn': len(hull.data.polygons),
              'triangles': G['tri_count'](hull), 'sockets': sock_names, 'slots': slots,
              'source': 'L9 kitbash of %s (%s)' % (base_key, recipe['note'])}
    with open(os.path.join(out_dir, 'hero.json'), 'w', encoding='utf-8') as f:
        json.dump(report, f, indent=1)
    entry = G['export_gltf'](hull, {'bcTint': None}, report, slots, out_dir, slug, True)
    return key, entry


def build_peregrine(key):
    fac, slug = key.split('.')[1], key.split('.', 2)[2]
    bpy.ops.wm.read_factory_settings(use_empty=True)
    hull = kb_recipes.peregrine_object()
    length = 30.0
    G['plating_unwrap'](hull, length)
    out_dir = os.path.join(SHIPS, key)
    os.makedirs(out_dir, exist_ok=True)
    # the painted vertex colour "Col" (hull, accents, canopy, nacelle grilles) is made by the recipe; plating_unwrap cleared
    # the material list, so paint after unwrapping
    kb_recipes.peregrine_paint(hull)
    slots = [{'name': 'M00', 'plating': 'fed', 'vertexBake': True, 'bc': None, 'n': None, 'orm': None, 'e': None, 'tint': None,
              'metallic': 0.5, 'emissive': 0.0}]
    bpy.ops.object.select_all(action='DESELECT')
    hull.select_set(True)
    bpy.context.view_layer.objects.active = hull
    bpy.ops.object.shade_smooth_by_angle(angle=math.radians(35), keep_sharp_edges=True)
    hull.name = hull.data.name = 'SM_%s' % slug
    names = []
    for name, at in kb_recipes.PEREGRINE_SOCKETS:
        e = bpy.data.objects.new('SOCKET_' + name, None)
        e.location = Vector(at)
        e.empty_display_size = 0.3
        bpy.context.scene.collection.objects.link(e)
        e.parent = hull
        names.append(name)
    report = {'key': key, 'faction': fac, 'file': 'SM_%s.glb' % slug, 'lengthMeters': length, 'trianglesIn': len(hull.data.polygons),
              'triangles': G['tri_count'](hull), 'sockets': names, 'slots': slots,
              'source': 'L9 original model: 30 m Peregrine-class fighter (wedge hull, twin nacelles), built in Blender from primitives'}
    with open(os.path.join(out_dir, 'hero.json'), 'w', encoding='utf-8') as f:
        json.dump(report, f, indent=1)
    entry = G['export_gltf'](hull, {'bcTint': None}, report, slots, out_dir, slug, True)
    return key, entry


def main():
    pats = (arg('--keys') or '*').split(',')
    entries = {}
    for key, recipe in kb_recipes.RECIPES.items():
        if not any(fnmatch.fnmatch(key, p) for p in pats):
            continue
        try:
            k, e = build_kitbash(key, recipe)
            entries[k] = e
            print('KB OK  %s from %s' % (k, recipe['base']), flush=True)
        except Exception as ex:
            import traceback
            traceback.print_exc()
            print('KB ERR %s %s' % (key, ex), flush=True)
    if any(fnmatch.fnmatch('ship.fed.peregrine', p) for p in pats):
        try:
            k, e = build_peregrine('ship.fed.peregrine')
            entries[k] = e
            print('KB OK  ship.fed.peregrine (original)', flush=True)
        except Exception as ex:
            import traceback
            traceback.print_exc()
            print('KB ERR peregrine %s' % ex, flush=True)
    if entries:
        sys.path.insert(0, TOOLS)
        import hero_gltf_manifest as hgm
        hgm.merge_manifest(os.path.join(SHIPS, hgm.MANIFEST_NAME), entries)
        print('manifest merged', len(entries))


main()
