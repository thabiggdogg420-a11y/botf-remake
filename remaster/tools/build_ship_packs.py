#!/usr/bin/env python3
"""Builds the hero-ship packs from the private LocalAssets/ships folder (M1 webparity; the models are third-party, so the packs
are build outputs only: never committed, never published).

  python remaster/tools/build_ship_packs.py --ships <LocalAssets/ships> --desktop <out/ships_desktop.pck> --web <out_dir>
      [--stage <dir>] [--godot <console exe>] [--proj remaster/godot] [--web-max 2048] [--jpeg-q 88]

Desktop: ships_desktop.pck holds every hero glb exactly as the dev run reads it, plus godot-hero-manifest.json and a path-free
copy of resolved-manifest.json (class lengths), all under res://assets/ships/. AppLoader mounts it from <exe dir>/packs or
user://packs; BattleAssets then finds the heroes in res:// with no LocalAssets folder and no environment variable.

Web: one pack per faction, ships_<fac>_web.pck (fed, kli, rom, car, fer), each with the manifests and that faction's glbs,
reduced for download: the decimated LOD1..3 meshes are dropped (HeroLook.min_lod_ratio = 1.0 hides them anyway, so the frame
is the same), and embedded textures are capped at --web-max px and re-encoded as JPEG (normal maps at a higher quality;
images with alpha stay PNG). The web client fetches them in the background after the title (AppLoader.fetch_background).

Each output is rebuilt only when an input is newer. Prints one line per pack with its size. Exit 0 when every pack exists.
"""
import argparse
import io
import json
import os
import re
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
FACTIONS = ["fed", "kli", "rom", "car", "fer"]
LOD_RE = re.compile(r"_LOD\d+$")


# ---------------------------------------------------------------------------------------------------------------------
# glb reading and writing
def read_glb(path):
    d = open(path, "rb").read()
    magic, _ver, _length = struct.unpack("<III", d[:12])
    if magic != 0x46546C67:
        raise ValueError("not a glb: %s" % path)
    off, js, binc = 12, None, b""
    while off < len(d):
        clen, ctype = struct.unpack("<II", d[off:off + 8])
        chunk = d[off + 8:off + 8 + clen]
        if ctype == 0x4E4F534A:
            js = json.loads(chunk.decode("utf-8"))
        elif ctype == 0x004E4942:
            binc = chunk
        off += 8 + clen
    return js, binc


def write_glb(path, js, binc):
    jb = json.dumps(js, separators=(",", ":")).encode("utf-8")
    jb += b" " * ((4 - len(jb) % 4) % 4)
    binc += b"\0" * ((4 - len(binc) % 4) % 4)
    total = 12 + 8 + len(jb) + (8 + len(binc) if binc else 0)
    with open(path, "wb") as f:
        f.write(struct.pack("<III", 0x46546C67, 2, total))
        f.write(struct.pack("<II", len(jb), 0x4E4F534A))
        f.write(jb)
        if binc:
            f.write(struct.pack("<II", len(binc), 0x004E4942))
            f.write(binc)


def _reencode(data, mime, max_px, quality):
    from PIL import Image
    im = Image.open(io.BytesIO(data))
    im.load()
    resized = False
    if max(im.size) > max_px:
        s = max_px / float(max(im.size))
        im = im.resize((max(1, round(im.width * s)), max(1, round(im.height * s))), Image.LANCZOS)
        resized = True
    alpha = im.mode in ("RGBA", "LA") or (im.mode == "P" and "transparency" in im.info)
    out = io.BytesIO()
    if alpha:
        im.save(out, "PNG", optimize=True)
        new_mime = "image/png"
    else:
        if im.mode not in ("RGB", "L"):
            im = im.convert("RGB")
        im.save(out, "JPEG", quality=quality, subsampling=0 if quality >= 92 else 2, optimize=True)
        new_mime = "image/jpeg"
    b = out.getvalue()
    if not resized and len(b) >= len(data):
        return data, mime
    return b, new_mime


def reduce_glb(src, dst, max_px, quality, normal_px=None, normal_q=95):
    """Writes a web copy of `src`: LOD meshes dropped, textures capped and re-encoded. Returns (old_bytes, new_bytes)."""
    js, binc = read_glb(src)
    nodes = js.get("nodes", [])
    for n in nodes:
        if "mesh" in n and LOD_RE.search(str(n.get("name", ""))):
            del n["mesh"]
    meshes = js.get("meshes", [])
    used_mesh = sorted({n["mesh"] for n in nodes if "mesh" in n})
    mesh_map = {old: i for i, old in enumerate(used_mesh)}
    for n in nodes:
        if "mesh" in n:
            n["mesh"] = mesh_map[n["mesh"]]
    js["meshes"] = [meshes[i] for i in used_mesh]

    used_acc = set()
    for m in js["meshes"]:
        for p in m.get("primitives", []):
            used_acc.update(p.get("attributes", {}).values())
            if "indices" in p:
                used_acc.add(p["indices"])
            for t in p.get("targets", []):
                used_acc.update(t.values())
    for s in js.get("skins", []):
        if "inverseBindMatrices" in s:
            used_acc.add(s["inverseBindMatrices"])
    for a in js.get("animations", []):
        for smp in a.get("samplers", []):
            used_acc.update([smp["input"], smp["output"]])
    accessors = js.get("accessors", [])
    used_acc = sorted(used_acc)
    acc_map = {old: i for i, old in enumerate(used_acc)}
    js["accessors"] = [accessors[i] for i in used_acc]
    for m in js["meshes"]:
        for p in m.get("primitives", []):
            p["attributes"] = {k: acc_map[v] for k, v in p.get("attributes", {}).items()}
            if "indices" in p:
                p["indices"] = acc_map[p["indices"]]
            if "targets" in p:
                p["targets"] = [{k: acc_map[v] for k, v in t.items()} for t in p["targets"]]
    for s in js.get("skins", []):
        if "inverseBindMatrices" in s:
            s["inverseBindMatrices"] = acc_map[s["inverseBindMatrices"]]
    for a in js.get("animations", []):
        for smp in a.get("samplers", []):
            smp["input"], smp["output"] = acc_map[smp["input"]], acc_map[smp["output"]]

    # Normal-map images get the higher JPEG quality.
    tex = js.get("textures", [])
    normal_imgs = set()
    for mat in js.get("materials", []):
        nt = mat.get("normalTexture")
        if nt is not None and nt.get("index", -1) < len(tex) and "source" in tex[nt["index"]]:
            normal_imgs.add(tex[nt["index"]]["source"])

    views = js.get("bufferViews", [])
    used_bv = set()
    for a in js["accessors"]:
        if "bufferView" in a:
            used_bv.add(a["bufferView"])
        sp = a.get("sparse")
        if sp:
            used_bv.update([sp["indices"]["bufferView"], sp["values"]["bufferView"]])
    img_bv = {}
    for i, im in enumerate(js.get("images", [])):
        if "bufferView" in im:
            used_bv.add(im["bufferView"])
            img_bv[im["bufferView"]] = i
    new_views, bv_map, out = [], {}, bytearray()
    for old in sorted(used_bv):
        v = dict(views[old])
        data = bytes(binc[v.get("byteOffset", 0):v.get("byteOffset", 0) + v["byteLength"]])
        if old in img_bv:
            im = js["images"][img_bv[old]]
            is_n = img_bv[old] in normal_imgs
            q = normal_q if is_n else quality
            data, im["mimeType"] = _reencode(data, im.get("mimeType", "image/png"), (normal_px or max_px) if is_n else max_px, q)
        while len(out) % 8:
            out.append(0)
        v["byteOffset"] = len(out)
        v["byteLength"] = len(data)
        v["buffer"] = 0
        out.extend(data)
        bv_map[old] = len(new_views)
        new_views.append(v)
    js["bufferViews"] = new_views
    for a in js["accessors"]:
        if "bufferView" in a:
            a["bufferView"] = bv_map[a["bufferView"]]
        sp = a.get("sparse")
        if sp:
            sp["indices"]["bufferView"] = bv_map[sp["indices"]["bufferView"]]
            sp["values"]["bufferView"] = bv_map[sp["values"]["bufferView"]]
    for im in js.get("images", []):
        if "bufferView" in im:
            im["bufferView"] = bv_map[im["bufferView"]]
    js["buffers"] = [{"byteLength": len(out)}]
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    write_glb(dst, js, bytes(out))
    return os.path.getsize(src), os.path.getsize(dst)


# ---------------------------------------------------------------------------------------------------------------------
def newer(out, inputs):
    if not os.path.isfile(out):
        return True
    t = os.path.getmtime(out)
    return any(os.path.getmtime(i) > t for i in inputs)


def pack(godot, proj, entries, out):
    out = os.path.abspath(out)
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    lst = out + ".list.txt"
    with open(lst, "w", encoding="utf-8") as f:
        for res, disk in entries:
            f.write("%s|%s\n" % (res, os.path.abspath(disk).replace(chr(92), "/")))
    r = subprocess.run([godot, "--headless", "--audio-driver", "Dummy", "--path", proj, "-s", "res://tools/pack_list.gd", "--", lst, out],
                       capture_output=True, text=True, creationflags=getattr(subprocess, "BELOW_NORMAL_PRIORITY_CLASS", 0))
    os.remove(lst)
    if r.returncode != 0 or not os.path.isfile(out):
        sys.exit("pack failed for %s:\n%s\n%s" % (out, r.stdout[-2000:], r.stderr[-2000:]))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--ships", required=True)
    ap.add_argument("--desktop")
    ap.add_argument("--web")
    ap.add_argument("--portraits", help="LocalAssets/portraits: race portraits packed as res://art/portrait/*.png (art_desktop.pck, art_web.pck)")
    ap.add_argument("--stage", default=None)
    ap.add_argument("--godot", default=os.environ.get("GODOT", r"C:\Tools\Godot\Godot_v4.5-stable_win64_console.exe"))
    ap.add_argument("--proj", default=os.path.normpath(os.path.join(HERE, "..", "godot")))
    ap.add_argument("--web-max", type=int, default=2048, help="longest side of a web texture (16 Nov: back to 2048, no total-download budget)")
    ap.add_argument("--normal-max", type=int, default=2048, help="longest side of a web normal map")
    ap.add_argument("--jpeg-q", type=int, default=90)
    ap.add_argument("--normal-q", type=int, default=92)
    a = ap.parse_args()
    ships = os.path.abspath(a.ships)
    man_path = os.path.join(ships, "godot-hero-manifest.json")
    heroes = json.load(open(man_path, encoding="utf-8"))["heroes"]
    stage = os.path.abspath(a.stage or os.path.join(a.proj, "build", "ships-stage"))
    os.makedirs(stage, exist_ok=True)
    # The stage sits inside the Godot project folder (build/ is gitignored): .gdignore keeps the editor from importing the
    # staged glbs (an import of 82 hulls is minutes of work and hundreds of MB of .godot cache).
    open(os.path.join(stage, ".gdignore"), "w").close()
    tool_inputs = [os.path.abspath(__file__)]

    # The resolved manifest carries the class lengths (BattleShip.length_meters_for); its file lists are disk paths of the
    # Unreal sources, which the packs never need: the copy keeps every field but those.
    resolved = os.path.join(os.path.dirname(ships), "resolved-manifest.json")
    res_copy = os.path.join(stage, "resolved-manifest.json")
    base_entries = [("res://assets/ships/godot-hero-manifest.json", man_path)]
    if os.path.isfile(resolved):
        if newer(res_copy, [resolved] + tool_inputs):
            rm = json.load(open(resolved, encoding="utf-8"))
            for rec in rm.get("assets", {}).values():
                if isinstance(rec, dict):
                    rec.pop("importFiles", None)
                    rec.pop("files", None)
            json.dump(rm, open(res_copy, "w", encoding="utf-8"), indent=1)
        base_entries.append(("res://assets/ships/resolved-manifest.json", res_copy))

    glbs = {}
    for key, e in heroes.items():
        rel = str(e.get("glb", "")).replace(chr(92), "/")
        src = os.path.join(ships, rel)
        if rel and os.path.isfile(src):
            glbs[key] = (rel, src, str(e.get("faction", key.split(".")[1] if "." in key else "fed")))
    print("heroes: %d in manifest, %d glbs found" % (len(heroes), len(glbs)))

    if a.portraits and os.path.isdir(a.portraits):
        pics = [(("res://art/portrait/" + f), os.path.join(a.portraits, f)) for f in sorted(os.listdir(a.portraits))
                if f.startswith("portrait.") and f.endswith(".png")]
        outs = ([os.path.join(os.path.dirname(os.path.abspath(a.desktop)), "art_desktop.pck")] if a.desktop else []) +                ([os.path.join(a.web, "art_web.pck")] if a.web else [])
        for out in outs:
            os.makedirs(os.path.dirname(out), exist_ok=True)
            if pics and newer(out, [d for _r, d in pics] + tool_inputs):
                pack(a.godot, a.proj, pics, out)
            print("%s %.1f MB (%d portraits)" % (os.path.basename(out), os.path.getsize(out) / 1048576.0, len(pics)))

    if a.desktop:
        entries = base_entries + [("res://assets/ships/" + rel, src) for rel, src, _f in glbs.values()]
        if newer(a.desktop, [d for _r, d in entries] + tool_inputs):
            pack(a.godot, a.proj, entries, a.desktop)
        print("%s %.1f MB (%d glbs)" % (os.path.basename(a.desktop), os.path.getsize(a.desktop) / 1048576.0, len(glbs)))

    if a.web:
        os.makedirs(a.web, exist_ok=True)
        before = after = 0
        web_files = {}
        for key, (rel, src, fac) in glbs.items():
            dst = os.path.join(stage, "web", rel)
            if newer(dst, [src] + tool_inputs):
                reduce_glb(src, dst, a.web_max, a.jpeg_q, a.normal_max, a.normal_q)
            before += os.path.getsize(src)
            after += os.path.getsize(dst)
            web_files[key] = [("res://assets/ships/" + rel, dst)]
        print("web glbs: %.1f MB -> %.1f MB" % (before / 1048576.0, after / 1048576.0))
        # F1: one pack per hull (hull_<key with dots as underscores>_web.pck) plus ships_base_web.pck with the manifests, so a
        # battle downloads only the classes on screen (a few MB each) and not a whole faction (up to 90 MB). Faction packs of
        # the earlier builds are removed so nothing stale is copied into the export.
        for old in os.listdir(a.web):
            if old.startswith("ships_") and old.endswith("_web.pck") and old != "ships_base_web.pck":
                os.remove(os.path.join(a.web, old))
        total = 0
        base_out = os.path.join(a.web, "ships_base_web.pck")
        if newer(base_out, [d for _r, d in base_entries] + tool_inputs):
            pack(a.godot, a.proj, base_entries, base_out)
        total += os.path.getsize(base_out)
        print("%s %.2f MB (manifests)" % (os.path.basename(base_out), os.path.getsize(base_out) / 1048576.0))
        for key in sorted(web_files):
            out = os.path.join(a.web, "hull_%s_web.pck" % key.replace(".", "_"))
            entries = web_files[key]
            if newer(out, [d for _r, d in entries] + tool_inputs):
                pack(a.godot, a.proj, entries, out)
            total += os.path.getsize(out)
        print("web hull packs: %d, total with manifests %.1f MB" % (len(web_files), total / 1048576.0))


if __name__ == "__main__":
    main()
