"""Blender headless conversion script for BOTF combat ship assets.

Imports static MilkShape 3D (MS3D) or Blender assets, standardizes coordinate orientation,
centers pivot at the bounding-box center, sets up PBR materials and packed textures, computes
exact bounding box and sphere radii, and exports game-ready GLB for Unreal Engine.

Run via:
blender.exe --background --python convert_combat_assets.py -- --source <input> --output <output.glb> --meta <meta.json>
"""
import sys
import os
from pathlib import Path
import struct
import math
import hashlib
import json

try:
    import bpy
    from mathutils import Vector
except ImportError:
    pass  # Allow importing for CLI help or non-Blender inspection


def parse_args():
    argv = sys.argv
    if "--" in argv:
        argv = argv[argv.index("--") + 1:]
    else:
        argv = []

    args = {
        "source": None,
        "output": None,
        "meta": None,
        "faction": "Federation",
        "author": "Icewolf132",
        "default_material": "auto",
        "target_class": "Unspecified",
        "status": "Ready",
        "source_rel": ""
    }
    i = 0
    while i < len(argv):
        arg = argv[i]
        if arg == "--source" and i + 1 < len(argv):
            args["source"] = argv[i + 1]
            i += 2
        elif arg == "--output" and i + 1 < len(argv):
            args["output"] = argv[i + 1]
            i += 2
        elif arg == "--meta" and i + 1 < len(argv):
            args["meta"] = argv[i + 1]
            i += 2
        elif arg == "--faction" and i + 1 < len(argv):
            args["faction"] = argv[i + 1]
            i += 2
        elif arg == "--author" and i + 1 < len(argv):
            args["author"] = argv[i + 1]
            i += 2
        elif arg == "--class" and i + 1 < len(argv):
            args["target_class"] = argv[i + 1]
            i += 2
        elif arg == "--status" and i + 1 < len(argv):
            args["status"] = argv[i + 1]
            i += 2
        elif arg == "--source-rel" and i + 1 < len(argv):
            args["source_rel"] = argv[i + 1]
            i += 2
        else:
            i += 1
    return args


class MS3DReader:
    def __init__(self, data: bytes):
        self.data = data
        self.offset = 0

    def read(self, fmt: str):
        fmt = '<' + fmt
        size = struct.calcsize(fmt)
        if self.offset + size > len(self.data):
            raise ValueError("Truncated MS3D buffer")
        res = struct.unpack_from(fmt, self.data, self.offset)
        self.offset += size
        return res

    def count(self) -> int:
        return self.read('H')[0]

    @staticmethod
    def decode_str(b: bytes) -> str:
        return b.split(b'\0', 1)[0].decode('cp1252', errors='ignore')


def create_pbr_material(name, diffuse_color, roughness=0.35, metallic=0.0, emissive_color=(0, 0, 0), emissive_strength=1.0):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    shader = nodes.get('Principled BSDF')
    shader.inputs['Base Color'].default_value = (*diffuse_color[:3], 1.0)
    shader.inputs['Metallic'].default_value = metallic
    shader.inputs['Roughness'].default_value = roughness
    shader.inputs['Emission Color'].default_value = (*emissive_color[:3], 1.0)
    shader.inputs['Emission Strength'].default_value = emissive_strength
    return mat, shader


def convert_ms3d_to_glb(source_path: Path, output_path: Path, faction: str, author: str, target_class: str, status: str = "Ready", source_rel: str = "") -> dict:
    source_bytes = source_path.read_bytes()
    source_sha256 = hashlib.sha256(source_bytes).hexdigest()
    reader = MS3DReader(source_bytes)

    magic, version = reader.read('10si')
    if magic != b'MS3D000000' or version != 4:
        raise ValueError(f"Expected MilkShape v4 file, got {magic!r} v{version}")

    # Read vertices: convert Milkshape (x, y, z) to Blender (x, -z, y)
    raw_vertices = []
    bound_vertices = 0
    for _ in range(reader.count()):
        flags, x, y, z, bone, refs = reader.read('B3fbB')
        if not (math.isfinite(x) and math.isfinite(y) and math.isfinite(z)):
            raise ValueError("Nonfinite vertex in MS3D")
        raw_vertices.append((x, -z, y))
        if bone != -1:
            bound_vertices += 1

    # Read triangles
    triangles = []
    for _ in range(reader.count()):
        row = reader.read('H3H9f3f3fBB')
        v_idx = row[1:4]
        if max(v_idx) >= len(raw_vertices):
            raise ValueError("Triangle vertex index out of range")
        normals = [
            (row[4], -row[6], row[5]),
            (row[7], -row[9], row[8]),
            (row[10], -row[12], row[11])
        ]
        s = row[13:16]
        t = row[16:19]
        triangles.append((v_idx, s, t, normals))

    # Read groups
    groups = []
    for _ in range(reader.count()):
        flags, gname, count = reader.read('B32sH')
        indices = reader.read(f"{count}H")
        mat_idx = reader.read('b')[0]
        if indices and max(indices) >= len(triangles):
            raise ValueError("Group triangle index out of range")
        groups.append((reader.decode_str(gname), indices, mat_idx))

    # Read materials
    materials = []
    for _ in range(reader.count()):
        row = reader.read('32s4f4f4f4fffB128s128s')
        materials.append({
            "name": reader.decode_str(row[0]),
            "ambient": row[1:5],
            "diffuse": row[5:9],
            "specular": row[9:13],
            "emissive": row[13:17],
            "shininess": row[17],
            "texture": reader.decode_str(row[20]),
            "alpha_texture": reader.decode_str(row[21])
        })

    # Read joints
    fps, current_frame, total_frames, n_joints = reader.read('ffiH')
    helper_joints = []
    has_anim = False
    for _ in range(n_joints):
        joint = reader.read('B32s32s3f3fHH')
        if joint[9] or joint[10]:
            has_anim = True
        helper_joints.append({
            "name": reader.decode_str(joint[1]),
            "parent": reader.decode_str(joint[2]),
            "rotation": list(joint[3:6]),
            "position": list(joint[6:9])
        })

    # Clear Blender scene
    bpy.ops.wm.read_factory_settings(use_empty=True)

    # Build Blender mesh
    mesh_name = source_path.stem
    mesh = bpy.data.meshes.new(mesh_name)
    mesh.from_pydata(raw_vertices, [], [t[0] for t in triangles])
    mesh.update()

    obj = bpy.data.objects.new(mesh_name, mesh)
    bpy.context.collection.objects.link(obj)
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)

    # Setup UVs and smoothing
    uv_layer = mesh.uv_layers.new(name="UVMap")
    for poly, (_, s_coords, t_coords, _) in zip(mesh.polygons, triangles):
        for corner, loop_idx in enumerate(poly.loop_indices):
            uv_layer.data[loop_idx].uv = (s_coords[corner], 1.0 - t_coords[corner])
        poly.use_smooth = True

    # Custom split normals
    split_normals = [norm for tri in triangles for norm in tri[3]]
    mesh.normals_split_custom_set(split_normals)

    # Setup materials
    used_textures = []
    texture_hashes = {}

    if materials:
        for mat_info in materials:
            diffuse = mat_info["diffuse"]
            emissive = mat_info["emissive"]
            shininess = mat_info["shininess"]
            roughness = min(1.0, math.sqrt(2.0 / (max(0, shininess) + 2.0)))
            mat, shader = create_pbr_material(
                name=mat_info["name"],
                diffuse_color=diffuse[:3],
                roughness=roughness,
                metallic=0.0,
                emissive_color=emissive[:3],
                emissive_strength=1.0 if any(e > 0.05 for e in emissive[:3]) else 0.0
            )

            tex_name = mat_info["texture"]
            if tex_name:
                filename = tex_name.replace('\\', '/').split('/')[-1]
                matches = [p for p in source_path.parent.iterdir() if p.name.lower() == filename.lower()]
                if not matches:
                    matches = [p for p in source_path.parent.parent.glob("**/*") if p.name.lower() == filename.lower()]

                if matches:
                    tex_file = matches[0]
                    tex_node = mat.node_tree.nodes.new('ShaderNodeTexImage')
                    tex_image = bpy.data.images.load(str(tex_file), check_existing=True)
                    tex_node.image = tex_image
                    mat.node_tree.links.new(tex_node.outputs['Color'], shader.inputs['Base Color'])
                    tex_image.pack()
                    used_textures.append(tex_file.name)
                    texture_hashes[tex_file.name] = hashlib.sha256(tex_file.read_bytes()).hexdigest()

            obj.data.materials.append(mat)

        for gname, indices, mat_idx in groups:
            slot = max(0, min(mat_idx, len(materials) - 1)) if mat_idx >= 0 else 0
            for idx in indices:
                mesh.polygons[idx].material_index = slot
    else:
        # Mesh had no authored materials (e.g. concept hull / fighter)
        if faction.lower() == "klingon":
            hull_mat, _ = create_pbr_material(
                name="Klingon_Tactical_Armor",
                diffuse_color=(0.12, 0.26, 0.14),
                roughness=0.38,
                metallic=0.55,
                emissive_color=(0.18, 1.0, 0.28),
                emissive_strength=0.2
            )
        elif faction.lower() == "romulan":
            hull_mat, _ = create_pbr_material(
                name="Romulan_Tactical_Armor",
                diffuse_color=(0.12, 0.34, 0.20),
                roughness=0.32,
                metallic=0.45
            )
        else:
            hull_mat, _ = create_pbr_material(
                name="Federation_Tactical_Alloy",
                diffuse_color=(0.38, 0.47, 0.55),
                roughness=0.35,
                metallic=0.40
            )
        obj.data.materials.append(hull_mat)
        for poly in mesh.polygons:
            poly.material_index = 0

    # Center mesh at origin: bounding-box center
    min_co = [min(v[i] for v in raw_vertices) for i in range(3)]
    max_co = [max(v[i] for v in raw_vertices) for i in range(3)]
    center = Vector([(min_co[i] + max_co[i]) / 2.0 for i in range(3)])

    for vert in mesh.vertices:
        vert.co -= center

    mesh.update()

    # Calculate centered bounding box and bounding sphere radius
    centered_min = [min(v.co[i] for v in mesh.vertices) for i in range(3)]
    centered_max = [max(v.co[i] for v in mesh.vertices) for i in range(3)]
    dimensions = [centered_max[i] - centered_min[i] for i in range(3)]
    sphere_radius = max(v.co.length for v in mesh.vertices)

    output_path.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.export_scene.gltf(
        filepath=str(output_path),
        export_format='GLB',
        use_selection=True,
        export_yup=True
    )

    out_bytes = output_path.read_bytes()
    out_sha256 = hashlib.sha256(out_bytes).hexdigest()

    meta = {
        "candidate_name": source_path.stem,
        "target_class": target_class,
        "canonical_faction": faction,
        "source_file": source_path.name,
        "source_relative_path": source_rel if source_rel else str(source_path),
        "source_sha256": source_sha256,
        "output_glb": output_path.name,
        "output_glb_sha256": out_sha256,
        "output_size_bytes": len(out_bytes),
        "author": author,
        "triangles": len(triangles),
        "vertices": len(raw_vertices),
        "mesh_groups": len(groups),
        "material_count": len(obj.data.materials),
        "textures": used_textures,
        "texture_hashes": texture_hashes,
        "bounds": {
            "min": list(centered_min),
            "max": list(centered_max),
            "dimensions_xyz": list(dimensions),
            "bounding_sphere_radius": float(sphere_radius),
            "bounding_box_center_offset": list(center)
        },
        "coordinate_transform": {
            "source_axes": ["x", "-z", "y"],
            "pivot": "bounding_box_center",
            "gltf_export_y_up": True
        },
        "helper_joints": helper_joints,
        "status": status
    }
    return meta


def process_glb_asset(source_path: Path, output_path: Path, faction: str, author: str, target_class: str, status: str = "Ready", source_rel: str = "") -> dict:
    source_bytes = source_path.read_bytes()
    source_sha256 = hashlib.sha256(source_bytes).hexdigest()

    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=str(source_path))

    mesh_objs = [obj for obj in bpy.context.scene.objects if obj.type == 'MESH']
    if not mesh_objs:
        raise ValueError(f"No mesh objects found in {source_path}")

    bpy.ops.object.select_all(action='DESELECT')
    for obj in mesh_objs:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = mesh_objs[0]
    if len(mesh_objs) > 1:
        bpy.ops.object.join()
    obj = bpy.context.view_layer.objects.active
    mesh = obj.data

    # Center origin: bounding-box center
    verts = [v.co for v in mesh.vertices]
    min_co = [min(v[i] for v in verts) for i in range(3)]
    max_co = [max(v[i] for v in verts) for i in range(3)]
    center = Vector([(min_co[i] + max_co[i]) / 2.0 for i in range(3)])
    for vert in mesh.vertices:
        vert.co -= center
    mesh.update()

    centered_min = [min(v.co[i] for v in mesh.vertices) for i in range(3)]
    centered_max = [max(v.co[i] for v in mesh.vertices) for i in range(3)]
    dimensions = [centered_max[i] - centered_min[i] for i in range(3)]
    sphere_radius = max(v.co.length for v in mesh.vertices)

    output_path.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.export_scene.gltf(
        filepath=str(output_path),
        export_format='GLB',
        use_selection=True,
        export_yup=True
    )

    out_bytes = output_path.read_bytes()
    out_sha256 = hashlib.sha256(out_bytes).hexdigest()

    return {
        "candidate_name": output_path.stem,
        "target_class": target_class,
        "canonical_faction": faction,
        "source_file": source_path.name,
        "source_relative_path": source_rel if source_rel else str(source_path),
        "source_sha256": source_sha256,
        "output_glb": output_path.name,
        "output_glb_sha256": out_sha256,
        "output_size_bytes": len(out_bytes),
        "author": author,
        "triangles": len(mesh.polygons),
        "vertices": len(mesh.vertices),
        "mesh_groups": len(mesh_objs),
        "material_count": len(obj.data.materials),
        "textures": [],
        "texture_hashes": {},
        "bounds": {
            "min": list(centered_min),
            "max": list(centered_max),
            "dimensions_xyz": list(dimensions),
            "bounding_sphere_radius": float(sphere_radius),
            "bounding_box_center_offset": list(center)
        },
        "coordinate_transform": {
            "source_axes": ["x", "y", "z"],
            "pivot": "bounding_box_center",
            "gltf_export_y_up": True
        },
        "helper_joints": [],
        "status": status
    }


def main():
    args = parse_args()
    if not args["source"] or not args["output"]:
        print("Usage: blender.exe --background --python convert_combat_assets.py -- --source <file> --output <out.glb>")
        sys.exit(1)

    source_path = Path(args["source"]).resolve()
    output_path = Path(args["output"]).resolve()
    meta_path = Path(args["meta"]).resolve() if args["meta"] else None

    if not source_path.is_file():
        raise FileNotFoundError(f"Source file not found: {source_path}")

    ext = source_path.suffix.lower()
    if ext == ".ms3d":
        meta = convert_ms3d_to_glb(
            source_path=source_path,
            output_path=output_path,
            faction=args["faction"],
            author=args["author"],
            target_class=args["target_class"],
            status=args["status"],
            source_rel=args["source_rel"]
        )
    elif ext in (".glb", ".gltf"):
        meta = process_glb_asset(
            source_path=source_path,
            output_path=output_path,
            faction=args["faction"],
            author=args["author"],
            target_class=args["target_class"],
            status=args["status"],
            source_rel=args["source_rel"]
        )
    else:
        raise ValueError(f"Unsupported format: {ext}")

    if meta_path:
        meta_path.parent.mkdir(parents=True, exist_ok=True)
        meta_path.write_text(json.dumps(meta, indent=2), encoding='utf-8')

    print("BOTF_COMBAT_ASSET_CONVERT_SUCCESS", json.dumps({
        "name": meta["candidate_name"],
        "glb": meta["output_glb"],
        "triangles": meta["triangles"],
        "radius": meta["bounds"]["bounding_sphere_radius"],
        "status": meta["status"]
    }))


if __name__ == "__main__":
    main()
