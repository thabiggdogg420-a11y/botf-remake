"""Running lights and nav sockets for every hero hull (L9, art wave 2). Edits the hero glb in place (pure Python glTF edit,
no Blender round trip, so the baked BC/N/ORM/E materials and LODs stay byte-identical).

    python tools/art/add_nav_lights.py [--keys ship.fed.*,ship.kli.*] [--check]

Adds to LocalAssets/ships/<key>/SM_<slug>.glb, as top-level nodes next to the SOCKET_* empties (frame: +X nose, +Y up,
+Z starboard, metres):
  NAV_PORT   red steady, at the port-most point of the hull          NAV_STBD   green steady, starboard-most point
  NAV_AFT    white steady, aft-most point on the centreline
  STROBE_DORSAL / STROBE_VENTRAL   white strobes at the top and bottom of the hull
  STROBE_AFT_PORT / STROBE_AFT_STBD  (hulls over 150 m) white strobes at the aft wing tips
Each light is a small emissive icosphere (KHR_materials_emissive_strength, so it blooms under the glow clamps) carrying
extras {"botf_light": "nav"|"strobe", "period": s, "phase": s, "on": s}. The turntable animates strobes from the extras;
the game applies the same to any node whose extras carry botf_light (see ART-BIBLE running-lights note).
Also writes godot-hero-manifest.json: heroes[key].lights = [{node, kind, period, phase, on}], and the window glow:
slots with an E texture get emissiveStrength raised (art2_orig_emissive keeps the first value so reruns are stable).
"""
import fnmatch
import json
import math
import os
import struct
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import _common as C

SHIPS = os.path.join(C.LOCAL, 'ships')
MAN = os.path.join(SHIPS, 'godot-hero-manifest.json')
TAG = 'botf_art2_light'


def read_glb(path):
    b = open(path, 'rb').read()
    magic, ver, ln = struct.unpack('<4sII', b[:12])
    assert magic == b'glTF'
    off = 12
    js, binc = None, b''
    while off < ln:
        cl, ct = struct.unpack('<II', b[off:off + 8])
        data = b[off + 8: off + 8 + cl]
        if ct == 0x4E4F534A:
            js = json.loads(data.decode('utf8'))
        elif ct == 0x004E4942:
            binc = data
        off += 8 + cl
    return js, bytearray(binc)


def write_glb(path, js, binc):
    jb = json.dumps(js, separators=(',', ':')).encode('utf8')
    jb += b' ' * ((4 - len(jb) % 4) % 4)
    binc = bytes(binc) + b'\0' * ((4 - len(binc) % 4) % 4)
    total = 12 + 8 + len(jb) + 8 + len(binc)
    with open(path, 'wb') as f:
        f.write(struct.pack('<4sII', b'glTF', 2, total))
        f.write(struct.pack('<II', len(jb), 0x4E4F534A) + jb)
        f.write(struct.pack('<II', len(binc), 0x004E4942) + bytes(binc))


def node_matrix(n):
    if 'matrix' in n:
        return np.array(n['matrix'], np.float64).reshape(4, 4).T
    t = np.array(n.get('translation', [0, 0, 0]), np.float64)
    q = np.array(n.get('rotation', [0, 0, 0, 1]), np.float64)
    s = np.array(n.get('scale', [1, 1, 1]), np.float64)
    x, y, z, w = q
    R = np.array([[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                  [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                  [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]])
    M = np.eye(4)
    M[:3, :3] = R * s
    M[:3, 3] = t
    return M


def accessor_data(js, binc, ai):
    a = js['accessors'][ai]
    bv = js['bufferViews'][a['bufferView']]
    n = a['count']
    comp = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4}[a['type']]
    stride = bv.get('byteStride', comp * 4)
    start = bv.get('byteOffset', 0) + a.get('byteOffset', 0)
    raw = np.frombuffer(bytes(binc[start: start + stride * (n - 1) + comp * 4]), np.uint8)
    arr = np.lib.stride_tricks.as_strided(raw, shape=(n, comp * 4), strides=(stride, 1)).copy()
    return arr.view(np.float32).reshape(n, comp)


def world_vertices(js, binc, node_name):
    """All vertices of the named node (and its mesh children) in scene space."""
    nodes = js['nodes']
    out = []

    def walk(i, M):
        n = nodes[i]
        M2 = M @ node_matrix(n)
        if 'mesh' in n:
            for pr in js['meshes'][n['mesh']]['primitives']:
                v = accessor_data(js, binc, pr['attributes']['POSITION']).astype(np.float64)
                out.append((np.c_[v, np.ones(len(v))] @ M2.T)[:, :3])
        for c in n.get('children', []):
            walk(c, M2)
    for r in js['scenes'][js.get('scene', 0)]['nodes']:
        if nodes[r].get('name') == node_name or node_name is None:
            walk(r, np.eye(4))
    return np.concatenate(out) if out else None


def icosphere(sub=1):
    t = (1 + 5 ** 0.5) / 2
    v = [(-1, t, 0), (1, t, 0), (-1, -t, 0), (1, -t, 0), (0, -1, t), (0, 1, t), (0, -1, -t), (0, 1, -t), (t, 0, -1), (t, 0, 1), (-t, 0, -1), (-t, 0, 1)]
    f = [(0, 11, 5), (0, 5, 1), (0, 1, 7), (0, 7, 10), (0, 10, 11), (1, 5, 9), (5, 11, 4), (11, 10, 2), (10, 7, 6), (7, 1, 8),
         (3, 9, 4), (3, 4, 2), (3, 2, 6), (3, 6, 8), (3, 8, 9), (4, 9, 5), (2, 4, 11), (6, 2, 10), (8, 6, 7), (9, 8, 1)]
    v = [list(np.array(a) / np.linalg.norm(a)) for a in v]
    for _ in range(sub):
        cache, nf = {}, []

        def mid(a, b):
            k = (min(a, b), max(a, b))
            if k not in cache:
                m = (np.array(v[a]) + np.array(v[b])) / 2
                v.append(list(m / np.linalg.norm(m)))
                cache[k] = len(v) - 1
            return cache[k]
        for a, b, c in f:
            ab, bc, ca = mid(a, b), mid(b, c), mid(c, a)
            nf += [(a, ab, ca), (b, bc, ab), (c, ca, bc), (ab, bc, ca)]
        f = nf
    return np.array(v, np.float32), np.array(f, np.uint16).reshape(-1)


def strip_old(js):
    """Remove nodes from a previous run (tagged by extras) so reruns are stable."""
    nodes = js['nodes']
    drop = {i for i, n in enumerate(nodes) if n.get('extras', {}).get(TAG)}
    if not drop:
        return
    for sc in js['scenes']:
        sc['nodes'] = [i for i in sc['nodes'] if i not in drop]
    # keep indices stable: orphan the nodes (no scene reference); rename so they cannot be found by name
    for i in drop:
        nodes[i] = {'name': '_removed_light_%d' % i}


def lights_for(V, length):
    """Light positions (name, kind, rgb, point, period, phase, on) from the hull's own extremes."""
    x, y, z = V[:, 0], V[:, 1], V[:, 2]
    w = max(z.max() - z.min(), 1e-6)
    r = float(np.clip(length * 0.0030, 0.10, 1.6))

    def extreme(score, k=24):
        idx = np.argsort(score)[:k]
        return V[idx].mean(0)
    # wing tips: among the forward 70 percent of the hull so engines pods at the far aft do not steal the light
    fwd = x > (x.min() + 0.25 * (x.max() - x.min()))
    big = np.where(fwd)[0]
    port = V[big[np.argsort(z[big])[:24]]].mean(0)
    stbd = V[big[np.argsort(-z[big])[:24]]].mean(0)
    port[2] -= r * 1.3
    stbd[2] += r * 1.3
    centre = np.abs(z) < 0.12 * w
    cidx = np.where(centre)[0] if centre.sum() > 50 else np.arange(len(V))
    top = V[cidx[np.argsort(-y[cidx])[:24]]].mean(0)
    bot = V[cidx[np.argsort(y[cidx])[:24]]].mean(0)
    aft = V[cidx[np.argsort(x[cidx])[:24]]].mean(0)
    top[1] += r * 1.2
    bot[1] -= r * 1.2
    aft[0] -= r * 1.2
    L = [('NAV_PORT', 'nav', (1.0, 0.04, 0.02), port, 0, 0, 0),
         ('NAV_STBD', 'nav', (0.05, 1.0, 0.18), stbd, 0, 0, 0),
         ('NAV_AFT', 'nav', (1.0, 0.97, 0.9), aft, 0, 0, 0),
         ('STROBE_DORSAL', 'strobe', (1.0, 1.0, 1.0), top, 1.5, 0.0, 0.08),
         ('STROBE_VENTRAL', 'strobe', (1.0, 1.0, 1.0), bot, 1.5, 0.75, 0.08)]
    if length > 150:
        side = np.abs(z) > 0.45 * w
        sidx = np.where(side)[0]
        if len(sidx) > 40:
            ap = V[sidx[np.argsort(x[sidx] - 100 * (z[sidx] < 0))[:24]]].mean(0) if (z[sidx] < 0).any() else port
            pa = sidx[z[sidx] < 0]
            sa = sidx[z[sidx] > 0]
            if len(pa) > 20 and len(sa) > 20:
                ap = V[pa[np.argsort(x[pa])[:24]]].mean(0)
                asb = V[sa[np.argsort(x[sa])[:24]]].mean(0)
                ap[2] -= r * 1.2
                asb[2] += r * 1.2
                L += [('STROBE_AFT_PORT', 'strobe', (1, 1, 1), ap, 1.5, 0.35, 0.08), ('STROBE_AFT_STBD', 'strobe', (1, 1, 1), asb, 1.5, 1.1, 0.08)]
    return L, r


def add_lights(path, length, write=True):
    js, binc = read_glb(path)
    strip_old(js)
    nodes = js['nodes']
    lod0 = None
    for i in js['scenes'][js.get('scene', 0)]['nodes']:
        nm = nodes[i].get('name', '')
        if nm.startswith('SM_') and '_LOD' not in nm:
            lod0 = nm
            break
    V = world_vertices(js, binc, lod0)
    if V is None:
        raise RuntimeError('no mesh named SM_* in ' + path)
    lights, r = lights_for(V, length)
    # shared sphere geometry (positions + normals + indices) appended to the binary chunk
    pos, idx = icosphere(1)
    base = len(binc)
    binc += pos.astype('<f4').tobytes()
    binc += b'\0' * ((4 - len(binc) % 4) % 4)
    nbase = len(binc)
    binc += pos.astype('<f4').tobytes()  # unit sphere: normal = position
    ibase = len(binc)
    binc += idx.astype('<u2').tobytes()
    binc += b'\0' * ((4 - len(binc) % 4) % 4)
    js.setdefault('bufferViews', [])
    js.setdefault('accessors', [])
    bvs = len(js['bufferViews'])
    js['bufferViews'] += [{'buffer': 0, 'byteOffset': base, 'byteLength': pos.nbytes, 'target': 34962},
                          {'buffer': 0, 'byteOffset': nbase, 'byteLength': pos.nbytes, 'target': 34962},
                          {'buffer': 0, 'byteOffset': ibase, 'byteLength': idx.nbytes, 'target': 34963}]
    ac = len(js['accessors'])
    js['accessors'] += [{'bufferView': bvs, 'componentType': 5126, 'count': len(pos), 'type': 'VEC3', 'min': [-1, -1, -1], 'max': [1, 1, 1]},
                        {'bufferView': bvs + 1, 'componentType': 5126, 'count': len(pos), 'type': 'VEC3'},
                        {'bufferView': bvs + 2, 'componentType': 5123, 'count': len(idx), 'type': 'SCALAR'}]
    js['buffers'][0]['byteLength'] = len(binc)
    exts = js.setdefault('extensionsUsed', [])
    if 'KHR_materials_emissive_strength' not in exts:
        exts.append('KHR_materials_emissive_strength')
    mats = js.setdefault('materials', [])
    meshes = js.setdefault('meshes', [])
    mat_of = {}
    mesh_of = {}
    manifest_lights = []
    for name, kind, rgb, pt, period, phase, on in lights:
        key = tuple(rgb)
        if key not in mat_of:
            mat_of[key] = len(mats)
            mats.append({'name': 'LIGHT_%02d%02d%02d' % tuple(int(c * 99) for c in rgb),
                         'pbrMetallicRoughness': {'baseColorFactor': [rgb[0] * 0.1, rgb[1] * 0.1, rgb[2] * 0.1, 1.0], 'metallicFactor': 0.0, 'roughnessFactor': 0.5},
                         'emissiveFactor': list(rgb),
                         'extensions': {'KHR_materials_emissive_strength': {'emissiveStrength': 14.0 if kind == 'nav' else 30.0}}})
            mesh_of[key] = len(meshes)
            meshes.append({'name': 'LIGHT_MESH_%d' % mat_of[key],
                           'primitives': [{'attributes': {'POSITION': ac, 'NORMAL': ac + 1}, 'indices': ac + 2, 'material': mat_of[key], 'mode': 4}]})
        extras = {TAG: True, 'botf_light': kind, 'period': period, 'phase': phase, 'on': on}
        nodes.append({'name': name, 'mesh': mesh_of[key], 'translation': [float(pt[0]), float(pt[1]), float(pt[2])],
                      'scale': [r, r, r], 'extras': extras})
        js['scenes'][js.get('scene', 0)]['nodes'].append(len(nodes) - 1)
        manifest_lights.append({'node': name, 'kind': kind, 'period': period, 'phase': phase, 'on': on})
    if write:
        write_glb(path, js, binc)
    return manifest_lights, r


def main():
    pats = sys.argv[sys.argv.index('--keys') + 1].split(',') if '--keys' in sys.argv else None
    man = C.load_json(MAN)
    n = 0
    for key, e in sorted(man['heroes'].items()):
        if pats and not any(fnmatch.fnmatch(key, p) for p in pats):
            continue
        path = os.path.join(SHIPS, e['glb'].replace('/', os.sep))
        if not os.path.exists(path):
            print('skip (no glb)', key)
            continue
        if '--emissive-only' in sys.argv:
            ml, r = e.get('lights', []), 0.0
        else:
            ml, r = add_lights(path, float(e['lengthMeters']), '--check' not in sys.argv)
        e['lights'] = ml
        for s in e.get('slots', []):
            if 'art2_orig_emissive' not in s:
                s['art2_orig_emissive'] = s.get('emissiveStrength', 1.5)
            if float(s.get('eCoverage', 0) or 0) > 0:
                # windows glow: x2 on slots whose E map lights under 5 percent, x1.3 above (already dimmed for coverage)
                s['emissiveStrength'] = round(min(4.0, s['art2_orig_emissive'] * (2.0 if float(s['eCoverage']) < 0.05 else 1.3)), 2)
        n += 1
        print('%-28s %d lights, radius %.2f m' % (key, len(ml), r))
    if '--check' not in sys.argv:
        with open(MAN, 'w', encoding='utf-8') as f:
            json.dump(man, f, indent=1)
    print(n, 'hulls')


if __name__ == '__main__':
    main()
