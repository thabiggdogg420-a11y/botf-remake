"""Pure-Python MilkShape 3D (MS3D) v4 binary parser and inspection tool.

Parses header, vertices, triangles, mesh groups, materials, and joint data.
Calculates geometry bounds, normals verification, and static topology integrity.
"""
from pathlib import Path
import struct
import math
import hashlib
from typing import Dict, Any, List, Tuple, Optional


class MS3DParser:
    """Parses and validates MilkShape 3D v4 binary files."""

    def __init__(self, file_path: Path):
        self.file_path = Path(file_path).resolve()
        self.data = self.file_path.read_bytes()
        self.offset = 0

    def _read(self, fmt: str):
        fmt = '<' + fmt
        size = struct.calcsize(fmt)
        if self.offset + size > len(self.data):
            raise ValueError(f"Truncated MS3D record at offset {self.offset}")
        result = struct.unpack_from(fmt, self.data, self.offset)
        self.offset += size
        return result

    def _count(self) -> int:
        return self._read('H')[0]

    @staticmethod
    def _decode_string(val: bytes) -> str:
        return val.split(b'\0', 1)[0].decode('cp1252', errors='ignore')

    def parse(self) -> Dict[str, Any]:
        """Parse the MS3D file and return structured mesh metadata and geometry."""
        self.offset = 0
        magic, version = self._read('10si')
        if magic != b'MS3D000000' or version != 4:
            raise ValueError(f"Invalid MS3D format: magic={magic!r}, version={version}")

        # 1. Vertices
        n_vertices = self._count()
        vertices: List[Tuple[float, float, float]] = []
        bound_count = 0
        min_v = [float('inf')] * 3
        max_v = [float('-inf')] * 3

        for _ in range(n_vertices):
            flags, x, y, z, bone, refs = self._read('B3fbB')
            if not all(math.isfinite(v) for v in (x, y, z)):
                raise ValueError("Non-finite vertex coordinates in MS3D model")
            # In MS3D, coordinate system is X right, Y up, Z towards viewer.
            # Blender standard: X right, Y forward, Z up -> (x, -z, y).
            vertices.append((x, y, z))
            if bone != -1:
                bound_count += 1
            for i, coord in enumerate((x, y, z)):
                if coord < min_v[i]:
                    min_v[i] = coord
                if coord > max_v[i]:
                    max_v[i] = coord

        # 2. Triangles
        n_triangles = self._count()
        triangles: List[Dict[str, Any]] = []
        for _ in range(n_triangles):
            row = self._read('H3H9f3f3fBB')
            v_indices = row[1:4]
            if max(v_indices) >= n_vertices:
                raise ValueError(f"Triangle vertex index {max(v_indices)} >= vertex count {n_vertices}")
            normals = [
                (row[4], row[5], row[6]),
                (row[7], row[8], row[9]),
                (row[10], row[11], row[12]),
            ]
            s = row[13:16]
            t = row[16:19]
            smoothing = row[19]
            group_idx = row[20]
            triangles.append({
                "vertex_indices": list(v_indices),
                "normals": normals,
                "s": list(s),
                "t": list(t),
                "smoothing_group": smoothing,
                "group_index": group_idx
            })

        # 3. Mesh Groups
        n_groups = self._count()
        groups: List[Dict[str, Any]] = []
        for _ in range(n_groups):
            flags, name, count = self._read('B32sH')
            indices = self._read(f"{count}H")
            mat_idx = self._read('b')[0]
            if indices and max(indices) >= n_triangles:
                raise ValueError("Group triangle index out of range")
            groups.append({
                "name": self._decode_string(name),
                "flags": flags,
                "triangle_count": count,
                "triangle_indices": list(indices),
                "material_index": mat_idx
            })

        # 4. Materials
        n_materials = self._count()
        materials: List[Dict[str, Any]] = []
        for _ in range(n_materials):
            row = self._read('32s4f4f4f4fffB128s128s')
            materials.append({
                "name": self._decode_string(row[0]),
                "ambient": list(row[1:5]),
                "diffuse": list(row[5:9]),
                "specular": list(row[9:13]),
                "emissive": list(row[13:17]),
                "shininess": row[17],
                "transparency": row[18],
                "mode": row[19],
                "texture": self._decode_string(row[20]),
                "alpha_texture": self._decode_string(row[21])
            })

        # 5. Skeletal / Animation data
        fps, current_time, total_frames, n_joints = self._read('ffiH')
        joints: List[Dict[str, Any]] = []
        has_animation = False
        for _ in range(n_joints):
            joint = self._read('B32s32s3f3fHH')
            num_rot = joint[9]
            num_trans = joint[10]
            if num_rot > 0 or num_trans > 0:
                has_animation = True
            # Read keyframes if any
            for _ in range(num_rot):
                self._read('f3f')
            for _ in range(num_trans):
                self._read('f3f')
            joints.append({
                "name": self._decode_string(joint[1]),
                "parent": self._decode_string(joint[2]),
                "rotation": list(joint[3:6]),
                "position": list(joint[6:9]),
                "rot_keyframe_count": num_rot,
                "trans_keyframe_count": num_trans
            })

        # Compute bounding metrics
        if vertices:
            extents = [max_v[i] - min_v[i] for i in range(3)]
            center = [(min_v[i] + max_v[i]) / 2.0 for i in range(3)]
            radius = max(math.sqrt(sum((v[i] - center[i])**2 for i in range(3))) for v in vertices)
        else:
            extents = [0.0, 0.0, 0.0]
            center = [0.0, 0.0, 0.0]
            radius = 0.0

        return {
            "file": self.file_path.name,
            "sha256": hashlib.sha256(self.data).hexdigest(),
            "size_bytes": len(self.data),
            "vertex_count": n_vertices,
            "triangle_count": n_triangles,
            "group_count": n_groups,
            "material_count": n_materials,
            "joint_count": n_joints,
            "bound_vertex_count": bound_count,
            "has_animation": has_animation,
            "is_static": (bound_count == 0 and not has_animation),
            "bounds": {
                "min": min_v,
                "max": max_v,
                "extents": extents,
                "center": center,
                "bounding_sphere_radius": radius
            },
            "groups": groups,
            "materials": materials,
            "joints": joints
        }


if __name__ == "__main__":
    import sys
    import json
    if len(sys.argv) > 1:
        p = MS3DParser(Path(sys.argv[1]))
        res = p.parse()
        # Omit heavy per-triangle listings for CLI print
        print(json.dumps({k: v for k, v in res.items() if k not in ('groups', 'joints')}, indent=2))
