"""Convert a 3D Studio (.3ds) mesh to Wavefront OBJ + MTL, which Blender 5.x imports natively.

Blender 5 no longer ships a 3DS importer, and the local base-model folder has a few legacy .3ds hulls.
Pure Python, no dependencies. Keeps object names, UVs and per-face materials with their diffuse,
bump, opacity and self-illumination maps. Vertices are written as stored (3DS keeps them in world space).

    python convert_3ds.py <in.3ds> [out.obj]
"""
import struct
import sys
from pathlib import Path

MAIN, EDIT, OBJECT, TRIMESH = 0x4D4D, 0x3D3D, 0x4000, 0x4100
VERTS, FACES, FACEMAT, UVS = 0x4110, 0x4120, 0x4130, 0x4140
MATERIAL, MAT_NAME, MAT_DIFFUSE = 0xAFFF, 0xA000, 0xA020
MAPS = {0xA200: "map_Kd", 0xA230: "map_Bump", 0xA210: "map_d", 0xA33D: "map_Ke", 0xA204: "map_Ks"}
MAP_FILE = 0xA300
COLOR_24, LIN_COLOR_24, COLOR_F, LIN_COLOR_F = 0x0011, 0x0012, 0x0010, 0x0013


def chunks(data, start, end):
    pos = start
    while pos + 6 <= end:
        cid, length = struct.unpack_from("<HI", data, pos)
        if length < 6 or pos + length > end:
            break
        yield cid, pos + 6, pos + length
        pos += length


def cstring(data, pos):
    end = data.index(b"\0", pos)
    return data[pos:end].decode("latin-1"), end + 1


def colour(data, start, end):
    for cid, s, _ in chunks(data, start, end):
        if cid in (COLOR_24, LIN_COLOR_24):
            return tuple(b / 255 for b in data[s:s + 3])
        if cid in (COLOR_F, LIN_COLOR_F):
            return struct.unpack_from("<3f", data, s)
    return (0.8, 0.8, 0.8)


def read_material(data, start, end):
    mat = {"name": "default", "Kd": (0.8, 0.8, 0.8), "maps": {}}
    for cid, s, e in chunks(data, start, end):
        if cid == MAT_NAME:
            mat["name"] = cstring(data, s)[0]
        elif cid == MAT_DIFFUSE:
            mat["Kd"] = colour(data, s, e)
        elif cid in MAPS:
            for sub, ss, _ in chunks(data, s, e):
                if sub == MAP_FILE:
                    mat["maps"][MAPS[cid]] = cstring(data, ss)[0]
    return mat


def read_trimesh(data, start, end):
    mesh = {"v": [], "uv": [], "f": [], "fmat": {}}
    for cid, s, e in chunks(data, start, end):
        if cid == VERTS:
            n = struct.unpack_from("<H", data, s)[0]
            mesh["v"] = [struct.unpack_from("<3f", data, s + 2 + i * 12) for i in range(n)]
        elif cid == UVS:
            n = struct.unpack_from("<H", data, s)[0]
            mesh["uv"] = [struct.unpack_from("<2f", data, s + 2 + i * 8) for i in range(n)]
        elif cid == FACES:
            n = struct.unpack_from("<H", data, s)[0]
            mesh["f"] = [struct.unpack_from("<3H", data, s + 2 + i * 8) for i in range(n)]
            for sub, ss, _ in chunks(data, s + 2 + n * 8, e):
                if sub == FACEMAT:
                    name, p = cstring(data, ss)
                    count = struct.unpack_from("<H", data, p)[0]
                    for i in range(count):
                        mesh["fmat"][struct.unpack_from("<H", data, p + 2 + i * 2)[0]] = name
    return mesh


def read_3ds(path):
    data = Path(path).read_bytes()
    objects, materials = [], []
    for cid, s, e in chunks(data, 0, len(data)):
        if cid != MAIN:
            continue
        for cid2, s2, e2 in chunks(data, s, e):
            if cid2 != EDIT:
                continue
            for cid3, s3, e3 in chunks(data, s2, e2):
                if cid3 == MATERIAL:
                    materials.append(read_material(data, s3, e3))
                elif cid3 == OBJECT:
                    name, p = cstring(data, s3)
                    for cid4, s4, e4 in chunks(data, p, e3):
                        if cid4 == TRIMESH:
                            objects.append((name, read_trimesh(data, s4, e4)))
    return objects, materials


def write_obj(objects, materials, out):
    out = Path(out)
    mtl = out.with_suffix(".mtl")
    lines = [f"mtllib {mtl.name}"]
    base = 1
    for name, m in objects:
        if not m["v"] or not m["f"]:
            continue
        lines.append(f"o {name.replace(' ', '_') or 'object'}")
        lines += [f"v {x:.6f} {y:.6f} {z:.6f}" for x, y, z in m["v"]]
        has_uv = len(m["uv"]) == len(m["v"])
        if has_uv:
            lines += [f"vt {u:.6f} {v:.6f}" for u, v in m["uv"]]
        current = None
        for i, (a, b, c) in enumerate(m["f"]):
            mat = m["fmat"].get(i, "default")
            if mat != current:
                lines.append(f"usemtl {mat.replace(' ', '_')}")
                current = mat
            ids = [base + a, base + b, base + c]
            lines.append("f " + " ".join(f"{k}/{k}" if has_uv else str(k) for k in ids))
        base += len(m["v"])
    out.write_text("\n".join(lines) + "\n", encoding="utf-8")
    mlines = []
    for mat in materials + [{"name": "default", "Kd": (0.8, 0.8, 0.8), "maps": {}}]:
        mlines.append(f"newmtl {mat['name'].replace(' ', '_')}")
        mlines.append("Kd {:.4f} {:.4f} {:.4f}".format(*mat["Kd"]))
        mlines += [f"{key} {value}" for key, value in mat["maps"].items()]
    mtl.write_text("\n".join(mlines) + "\n", encoding="utf-8")
    return sum(len(m["f"]) for _, m in objects)


if __name__ == "__main__":
    src = Path(sys.argv[1])
    dst = Path(sys.argv[2]) if len(sys.argv) > 2 else src.with_suffix(".obj")
    objs, mats = read_3ds(src)
    tris = write_obj(objs, mats, dst)
    print(f"convert_3ds: {src.name} -> {dst.name}: {len(objs)} objects, {tris} triangles, {len(mats)} materials")
