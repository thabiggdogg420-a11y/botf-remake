"""Run via Unreal's Python commandlet, with the intended .uproject selected."""
import unreal

target = "/Game/Maps/Galaxy"
if not unreal.EditorAssetLibrary.does_asset_exist(target):
    subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not subsystem.new_level(target):
        raise RuntimeError("Could not create Galaxy map")
    subsystem.save_current_level()
unreal.log("BOTF_MAP_READY " + target)
