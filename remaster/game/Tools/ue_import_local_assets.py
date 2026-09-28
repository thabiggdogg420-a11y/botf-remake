"""Unreal Editor Python: import resolved local assets into /Game/Local/...

Run through the commandlet (Import-Assets.ps1 -UnrealImport does this):
  UnrealEditor-Cmd.exe BOTFRemake.uproject -run=pythonscript -script="<path>/ue_import_local_assets.py" -unattended -nop4 -nosplash
The resolved manifest path comes from the BOTF_RESOLVED_MANIFEST environment variable (default
LocalAssets/resolved-manifest.json next to the project).

Requires the Python Editor Script Plugin (enabled by default in UE 5.x editor builds). Uses the classic
AssetImportTask API, which routes FBX/WAV through Interchange when Interchange is enabled in UE 5.8.
Everything imported lands under /Game/Local, which is gitignored (Content/Local/).
"""
import json
import os

import unreal

PROJECT_DIR = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
MANIFEST = os.environ.get("BOTF_RESOLVED_MANIFEST") or os.path.join(PROJECT_DIR, "LocalAssets", "resolved-manifest.json")


def mesh_options():
    opts = unreal.FbxImportUI()
    opts.import_mesh = True
    opts.import_as_skeletal = False
    opts.import_materials = True
    opts.import_textures = True
    opts.import_animations = False
    opts.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    sm = opts.static_mesh_import_data
    sm.combine_meshes = True
    sm.auto_generate_collision = False
    sm.generate_lightmap_u_vs = False
    sm.convert_scene = True
    sm.force_front_x_axis = False
    sm.import_uniform_scale = 1.0
    return opts


def task(src, dest, options=None):
    t = unreal.AssetImportTask()
    t.filename = src
    t.destination_path = dest
    t.automated = True
    t.replace_existing = True
    t.save = True
    if options is not None:
        t.options = options
    return t


def main():
    with open(MANIFEST, encoding="utf-8-sig") as f:
        resolved = json.load(f)
    tasks = []
    for key, entry in resolved.get("assets", {}).items():
        if entry.get("status") != "resolved":
            continue
        dest = entry.get("ueFolder")
        if not dest or not dest.startswith("/Game/Local"):
            continue
        for path in entry.get("importFiles", []):
            ext = os.path.splitext(path)[1].lower()
            if ext == ".fbx":
                tasks.append(task(path, dest, mesh_options()))
            elif ext in (".wav", ".png", ".tga", ".dds"):
                tasks.append(task(path, dest))
    if not tasks:
        unreal.log_warning("BOTF import: nothing resolved in " + MANIFEST)
        return
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    done = sum(len(t.imported_object_paths) for t in tasks)
    unreal.log("BOTF import: %d tasks, %d objects imported" % (len(tasks), done))
    report = {t.filename: list(t.imported_object_paths) for t in tasks}
    with open(os.path.join(os.path.dirname(MANIFEST), "ue-import-report.json"), "w", encoding="utf-8") as f:
        json.dump(report, f, indent=1)
    # Write the object paths Unreal really created back into the resolved manifest, so UBotfAssetRegistry loads
    # exactly those (the importer only predicted them). Only the primary asset per file is kept (materials and
    # textures created alongside a mesh are skipped by class name).
    changed = False
    for key, entry in resolved.get("assets", {}).items():
        paths = []
        for src in entry.get("importFiles", []):
            for obj in report.get(src, []):
                name = obj.rsplit("/", 1)[-1].split(".")[0]
                stem = os.path.splitext(os.path.basename(src))[0]
                if name.replace("_", "").lower() == stem.replace("_", "").replace(".", "").replace(" ", "").lower():
                    paths.append(obj)
        if paths:
            entry["ueAssets"] = paths
            changed = True
    if changed:
        with open(MANIFEST, "w", encoding="utf-8") as f:
            json.dump(resolved, f, indent=1)


main()
