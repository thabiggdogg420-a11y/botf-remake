"""Convert static MilkShape v4 meshes to textured GLB in Blender, retaining provenance.

Layout checked against Assimp's MS3DLoader.cpp. Animation is deliberately unsupported.
Original archives and readmes remain under research/reference-local/community.
Run with Blender --background --python this-file.
"""
from pathlib import Path
import struct
import json
import hashlib
import math
import bpy
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'research/reference-local/community/Icewolf132'
OUTPUT = ROOT / 'game/ArtSource/Community/Icewolf132'
OUTPUT.mkdir(parents=True, exist_ok=True)

class Reader:
    def __init__(self, path):
        self.data = path.read_bytes()
        self.offset = 0
    def read(self, fmt):
        fmt = '<' + fmt
        size = struct.calcsize(fmt)
        if self.offset + size > len(self.data):
            raise ValueError('Truncated MS3D record')
        result = struct.unpack_from(fmt, self.data, self.offset)
        self.offset += size
        return result
    def count(self):
        return self.read('H')[0]

def text(value):
    return value.split(b'\0', 1)[0].decode('cp1252')

def convert(path):
    r = Reader(path)
    magic, version = r.read('10si')
    if magic != b'MS3D000000' or version != 4:
        raise ValueError('Expected MilkShape v4')
    vertices = []
    bound_vertices = 0
    for _ in range(r.count()):
        flags, x, y, z, bone, refs = r.read('B3fbB')
        if not all(math.isfinite(v) for v in (x,y,z)):
            raise ValueError('Nonfinite vertex')
        vertices.append((x, -z, y))
        bound_vertices += bone != -1
    triangles = []
    for _ in range(r.count()):
        row = r.read('H3H9f3f3fBB')
        if max(row[1:4]) >= len(vertices):
            raise ValueError('Triangle vertex out of range')
        normals = [(row[4+i*3], -row[6+i*3], row[5+i*3]) for i in range(3)]
        triangles.append((row[1:4], row[13:16], row[16:19], normals))
    groups = []
    for _ in range(r.count()):
        flags, name, count = r.read('B32sH')
        indices = r.read(str(count) + 'H')
        material = r.read('b')[0]
        if indices and max(indices) >= len(triangles):
            raise ValueError('Group triangle out of range')
        groups.append((text(name), indices, material))
    materials = []
    for _ in range(r.count()):
        row = r.read('32s4f4f4f4fffB128s128s')
        materials.append((text(row[0]), row[5:9], text(row[20]), text(row[21]), row[13:17], row[17]))
    fps, current, frames, joints = r.read('ffiH')
    helper_joints = []
    for _ in range(joints):
        joint = r.read('B32s32s3f3fHH')
        if joint[9] or joint[10]:
            raise ValueError('Animated meshes need a skeletal importer')
        helper_joints.append(dict(name=text(joint[1]), parent=text(joint[2]),
            source_rotation=list(joint[3:6]), source_position=list(joint[6:9])))
    if bound_vertices:
        raise ValueError('Bound meshes need a skeletal importer')

    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    mesh = bpy.data.meshes.new(path.stem)
    mesh.from_pydata(vertices, [], [t[0] for t in triangles])
    mesh.update()
    obj = bpy.data.objects.new(path.stem, mesh)
    bpy.context.collection.objects.link(obj)
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    uv = mesh.uv_layers.new(name='UVMap')
    for poly, (_, ss, tt, normals) in zip(mesh.polygons, triangles):
        for corner, loop in enumerate(poly.loop_indices):
            uv.data[loop].uv = (ss[corner], 1 - tt[corner])
        poly.use_smooth = True
    mesh.normals_split_custom_set([normal for triangle in triangles for normal in triangle[3]])
    used_textures = []
    texture_fingerprints = {}
    for name, diffuse, texture, alpha, emissive, shininess in materials:
        mat = bpy.data.materials.new(name)
        mat.use_nodes = True
        shader = mat.node_tree.nodes.get('Principled BSDF')
        shader.inputs['Base Color'].default_value = (*diffuse[:3], 1)
        shader.inputs['Metallic'].default_value = 0.0
        shader.inputs['Roughness'].default_value = min(1.0, math.sqrt(2.0 / (max(0, shininess) + 2.0)))
        shader.inputs['Emission Color'].default_value = (*emissive[:3], 1)
        shader.inputs['Emission Strength'].default_value = 1.0
        if texture:
            filename = texture.replace('\\', '/').split('/')[-1]
            matches = [p for p in path.parent.iterdir() if p.name.lower() == filename.lower()]
            if len(matches) != 1:
                raise ValueError(f'Missing texture {filename} for {path}')
            tex = mat.node_tree.nodes.new('ShaderNodeTexImage')
            tex.image = bpy.data.images.load(str(matches[0]), check_existing=True)
            mat.node_tree.links.new(tex.outputs['Color'], shader.inputs['Base Color'])
            tex.image.pack()
            used_textures.append(filename)
            texture_fingerprints[filename] = hashlib.sha256(matches[0].read_bytes()).hexdigest()
        obj.data.materials.append(mat)
    for name, indices, material in groups:
        if material < -1 or material >= len(materials):
            raise ValueError('Invalid material slot')
        for index in indices:
            mesh.polygons[index].material_index = max(material, 0)
    # Centre all variants at the origin; preserve proportions and UV layout.
    centre = sum((Vector(v) for v in vertices), Vector()) / len(vertices)
    for vert in mesh.vertices:
        vert.co -= centre
    bpy.ops.export_scene.gltf(filepath=str(OUTPUT / (path.stem + '.glb')),
        export_format='GLB', use_selection=True, export_yup=True)
    return dict(source=str(path.relative_to(ROOT)), name=path.stem,
        sha256=hashlib.sha256(r.data).hexdigest(), triangles=len(triangles),
        vertices=len(vertices), textures=used_textures, author='Icewolf132',
        texture_sha256=texture_fingerprints,
        source_to_blender_axes=['x', '-z', 'y'],
        blender_center_subtracted=list(centre),
        gltf_export_y_up=True,
        static_helper_joints=helper_joints,
        conversion='Static geometry, authored per-corner normals, UVs, diffuse textures and emissive colors. Metallic=0; roughness approximated from source Phong shininess; no invented PBR texture maps or animation.')

records = []
for source in sorted((SOURCE / 'Completed').rglob('*.ms3d')):
    try:
        records.append(convert(source))
    except ValueError as error:
        records.append(dict(source=str(source.relative_to(ROOT)), skipped=str(error)))
(OUTPUT / 'conversion-manifest.json').write_text(json.dumps(records, indent=2), encoding='utf-8')
(OUTPUT / 'README-original.txt').write_bytes((SOURCE / 'README.txt').read_bytes())
print('BOTF_COMMUNITY_CONVERSION_COMPLETE', json.dumps(records))
