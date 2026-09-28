"""Import fleet studies and author the runtime space-glow material in the game project."""
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir())
tools = unreal.AssetToolsHelpers.get_asset_tools()
for name in ("Federation", "Klingon", "Romulan", "Cardassian", "Ferengi"):
    source = root / "ArtSource" / "FleetStudies" / (name + ".glb")
    if not source.is_file():
        raise RuntimeError("Missing fleet source " + str(source))
    expected = f"/Game/Presentation/{name}/StaticMeshes/{name}"
    if not unreal.EditorAssetLibrary.does_asset_exist(expected):
        task = unreal.AssetImportTask()
        task.filename = str(source)
        task.destination_path = "/Game/Presentation"
        task.automated = True
        task.save = True
        task.replace_existing = False
        tools.import_asset_tasks([task])
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    meshes = [a for a in registry.get_assets_by_path(f"/Game/Presentation/{name}", recursive=True)
              if str(a.asset_class_path.asset_name) == "StaticMesh"]
    if len(meshes) != 1:
        raise RuntimeError(f"Expected one joined {name} mesh, found {len(meshes)}")
    unreal.log("BOTF_FLEET_ASSET " + name + " " + str(meshes[0].get_asset().get_path_name()))

for source in sorted((root / "ArtSource" / "Community" / "Icewolf132").glob("*.glb")):
    expected = f"/Game/Presentation/Community/{source.stem}/StaticMeshes/{source.stem}"
    task = unreal.AssetImportTask()
    task.filename = str(source)
    task.destination_path = "/Game/Presentation/Community"
    task.automated = True
    task.save = True
    task.replace_existing = True
    tools.import_asset_tasks([task])
    unreal.log("BOTF_COMMUNITY_ASSET " + expected)

path = "/Game/Presentation/M_SpaceGlow"
if not unreal.EditorAssetLibrary.does_asset_exist(path):
    mat = tools.create_asset("M_SpaceGlow", "/Game/Presentation", unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    tint = unreal.MaterialEditingLibrary.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -400, 0)
    tint.set_editor_property("parameter_name", "Tint")
    tint.set_editor_property("default_value", unreal.LinearColor(0.25, 0.6, 1, 1))
    power = unreal.MaterialEditingLibrary.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -400, 160)
    power.set_editor_property("parameter_name", "Glow")
    power.set_editor_property("default_value", 4.0)
    multiply = unreal.MaterialEditingLibrary.create_material_expression(mat, unreal.MaterialExpressionMultiply, -150, 0)
    unreal.MaterialEditingLibrary.connect_material_expressions(tint, "", multiply, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(power, "", multiply, "B")
    unreal.MaterialEditingLibrary.connect_material_property(multiply, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    unreal.MaterialEditingLibrary.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat)
path = "/Game/Presentation/M_ShieldShell"
if not unreal.EditorAssetLibrary.does_asset_exist(path):
    mat = tools.create_asset("M_ShieldShell", "/Game/Presentation", unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    tint = unreal.MaterialEditingLibrary.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -500, 0)
    tint.set_editor_property("parameter_name", "Tint")
    tint.set_editor_property("default_value", unreal.LinearColor(0.12, 0.45, 1.0, 1.0))
    strength = unreal.MaterialEditingLibrary.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -500, 180)
    strength.set_editor_property("parameter_name", "Strength")
    strength.set_editor_property("default_value", 0.0)
    rim = unreal.MaterialEditingLibrary.create_material_expression(mat, unreal.MaterialExpressionFresnel, -500, 340)
    rim.set_editor_property("exponent", 3.0)
    rim.set_editor_property("base_reflect_fraction", 0.03)
    alpha = unreal.MaterialEditingLibrary.create_material_expression(mat, unreal.MaterialExpressionMultiply, -220, 220)
    unreal.MaterialEditingLibrary.connect_material_expressions(rim, "", alpha, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(strength, "", alpha, "B")
    unreal.MaterialEditingLibrary.connect_material_property(tint, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    unreal.MaterialEditingLibrary.connect_material_property(alpha, "", unreal.MaterialProperty.MP_OPACITY)
    unreal.MaterialEditingLibrary.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat)
unreal.log("BOTF_PRESENTATION_ASSETS_READY")
