"""Render the converted Ranger to inspect geometry, UVs and texture preservation."""
from pathlib import Path
import bpy
import sys
from mathutils import Vector
root = Path(__file__).resolve().parents[1]
name = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else 'franger'
if name not in ('franger', 'fUlysses', 'lcruise1', 'freaver1', 'freaver2', 'freaver3'):
    raise ValueError('Unknown converted model')
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
bpy.ops.import_scene.gltf(filepath=str(root / 'game/ArtSource/Community/Icewolf132' / (name + '.glb')))
objects = [o for o in bpy.context.scene.objects if o.type == 'MESH']
points = [o.matrix_world @ Vector(c) for o in objects for c in o.bound_box]
centre = (Vector(tuple(min(p[i] for p in points) for i in range(3))) + Vector(tuple(max(p[i] for p in points) for i in range(3)))) / 2
radius = max((p-centre).length for p in points)
bpy.ops.object.camera_add(location=centre+Vector((1.2,-1.5,1.0))*radius*1.8)
camera = bpy.context.object
camera.rotation_euler = (centre-camera.location).to_track_quat('-Z','Y').to_euler()
camera.data.type='ORTHO'
camera.data.ortho_scale=radius*2.5
bpy.context.scene.camera=camera
bpy.ops.object.light_add(type='AREA', location=centre+Vector((0,-1,2))*radius*2)
bpy.context.object.data.energy=radius*radius*200
bpy.context.object.data.shape='DISK'
bpy.context.object.data.size=radius*2
bpy.context.object.rotation_euler=(centre-bpy.context.object.location).to_track_quat('-Z','Y').to_euler()
scene=bpy.context.scene
scene.world.color=(0.12,0.12,0.12)
scene.render.engine='CYCLES'
scene.cycles.samples=24
scene.render.resolution_x=1000
scene.render.resolution_y=750
scene.render.resolution_percentage=100
scene.render.filepath=str(root/'setup/output'/('community-' + name + '-preview.png'))
bpy.ops.render.render(write_still=True)
