"""Import the existing Blender smoke-test mesh into PipelineCheck, not game content."""
import os
import unreal

source = os.path.abspath(os.path.join(unreal.Paths.project_dir(), "..", "output", "pipeline-check.glb"))
if not os.path.isfile(source):
    raise RuntimeError("Missing Blender export: " + source)
task = unreal.AssetImportTask()
task.filename = source
task.destination_path = "/Game/PipelineVerification"
task.destination_name = "PipelineCube"
task.automated = True
task.save = True
task.replace_existing = False
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
objects = task.get_objects()
meshes = [obj for obj in objects if isinstance(obj, unreal.StaticMesh)]
if not meshes:
    raise RuntimeError("No StaticMesh produced from Blender GLB")
for mesh in meshes:
    unreal.log("BOTF_BLENDER_UNREAL_IMPORT_OK " + mesh.get_path_name())
