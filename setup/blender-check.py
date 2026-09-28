"""Check Blender background scripting and a game-ready interchange export."""
from pathlib import Path
import bpy

output = Path(__file__).resolve().parent / "output"
output.mkdir(exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.mesh.primitive_cube_add()
bpy.context.object.name = "PipelineCheckCube"
bpy.ops.wm.save_as_mainfile(filepath=str(output / "pipeline-check.blend"))
bpy.ops.export_scene.gltf(filepath=str(output / "pipeline-check.glb"), export_format="GLB")
assert (output / "pipeline-check.glb").stat().st_size > 0
print("GAME_REMAKES_BLENDER_EXPORT_OK")
