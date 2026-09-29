"""Unreal Editor Python: imports the procedural ship library into /Game/Generated/Ships (original art).

    UnrealEditor-Cmd.exe BOTFRemake.uproject -run=pythonscript -script="<path>/ue_import_generated_ships.py" -unattended -nosplash -nullrhi

Inputs, all under ArtSource/Generated/Ships (Build-GeneratedArt.ps1 makes them):
  <fac>/<slug>.fbx, generated-ships.json   blender_generate_ships.py (vertex colour "Mask", SOCKET_ empties)
  T_HullPanels.png, T_HullNormal.png       generate_hull_textures.py
Creates:
  /Game/Generated/Ships/T_HullPanels, T_HullNormal
  /Game/Generated/Ships/M_ShipHull         lit hull driven by the vertex mask: R lit windows (panel texture B picks the
                                            window cells), G warm glow (impulse, bussards), B cool glow (warp), A accent.
  /Game/Generated/Ships/MI_Hull_<fac>      one colour scheme per faction prefix
  /Game/Generated/Ships/<fac>/SM_<slug>    one mesh per ship key, sockets kept (WPN_*, ENG_*), MI_Hull_<fac> assigned
ABotfCombatScene::ApplyShipMesh loads SM_<slug> for any ship without a local model. Idempotent (assets are replaced).
"""
import json
import os
import re

import unreal

PROJECT_DIR = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
ART = os.path.join(PROJECT_DIR, "ArtSource", "Generated", "Ships")
DEST = "/Game/Generated/Ships"
MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()

# prefix: (hull, accent, warm glow, cool glow, panel contrast, metallic)
C = unreal.LinearColor
SCHEMES = {
    "fed": (C(0.46, 0.48, 0.52), C(0.62, 0.16, 0.12), C(1.0, 0.22, 0.06), C(0.22, 0.52, 1.0), 0.30, 0.55),
    "kli": (C(0.17, 0.22, 0.17), C(0.36, 0.09, 0.07), C(1.0, 0.18, 0.04), C(1.0, 0.28, 0.08), 0.55, 0.45),
    "rom": (C(0.20, 0.36, 0.26), C(0.46, 0.52, 0.40), C(0.35, 1.0, 0.45), C(0.30, 1.0, 0.55), 0.40, 0.40),
    "car": (C(0.50, 0.41, 0.27), C(0.30, 0.24, 0.17), C(1.0, 0.50, 0.12), C(1.0, 0.78, 0.30), 0.45, 0.45),
    "fer": (C(0.52, 0.32, 0.17), C(0.30, 0.22, 0.13), C(1.0, 0.38, 0.08), C(0.30, 0.90, 0.40), 0.40, 0.55),
    "dom": (C(0.40, 0.36, 0.46), C(0.22, 0.18, 0.28), C(0.80, 0.32, 1.0), C(0.62, 0.40, 1.0), 0.40, 0.50),
    "borg": (C(0.10, 0.11, 0.10), C(0.18, 0.20, 0.18), C(0.20, 1.0, 0.30), C(0.20, 1.0, 0.30), 0.80, 0.70),
    "breen": (C(0.30, 0.36, 0.28), C(0.20, 0.24, 0.18), C(0.40, 1.0, 0.50), C(0.30, 0.80, 1.0), 0.45, 0.45),
    "thol": (C(0.55, 0.30, 0.14), C(0.80, 0.55, 0.20), C(1.0, 0.45, 0.08), C(1.0, 0.70, 0.20), 0.30, 0.80),
    "s8472": (C(0.46, 0.40, 0.30), C(0.30, 0.34, 0.22), C(0.90, 0.80, 0.30), C(0.60, 1.0, 0.40), 0.25, 0.10),
    "gorn": (C(0.34, 0.30, 0.22), C(0.40, 0.18, 0.10), C(1.0, 0.40, 0.10), C(0.90, 0.60, 0.20), 0.45, 0.45),
    "reman": (C(0.16, 0.17, 0.19), C(0.30, 0.34, 0.30), C(0.40, 1.0, 0.40), C(0.30, 1.0, 0.50), 0.55, 0.50),
    "hiro": (C(0.26, 0.26, 0.24), C(0.40, 0.20, 0.10), C(1.0, 0.35, 0.10), C(1.0, 0.55, 0.20), 0.55, 0.60),
    "kazon": (C(0.44, 0.36, 0.24), C(0.30, 0.20, 0.12), C(1.0, 0.45, 0.10), C(1.0, 0.60, 0.20), 0.60, 0.40),
    "vid": (C(0.34, 0.38, 0.30), C(0.50, 0.40, 0.20), C(0.90, 0.80, 0.30), C(0.40, 0.90, 0.60), 0.40, 0.40),
    "malon": (C(0.30, 0.28, 0.24), C(0.50, 0.40, 0.10), C(1.0, 0.70, 0.10), C(0.60, 1.0, 0.20), 0.60, 0.50),
    "xindi": (C(0.38, 0.34, 0.28), C(0.26, 0.28, 0.22), C(1.0, 0.55, 0.20), C(0.40, 0.80, 1.0), 0.40, 0.50),
    "vul": (C(0.56, 0.46, 0.32), C(0.50, 0.26, 0.12), C(1.0, 0.45, 0.12), C(0.40, 0.60, 1.0), 0.30, 0.50),
    "_": (C(0.46, 0.46, 0.49), C(0.30, 0.30, 0.36), C(1.0, 0.45, 0.12), C(0.30, 0.75, 1.0), 0.40, 0.50),
}


def asset_name(slug):
    return "SM_" + re.sub(r"[^A-Za-z0-9_]", "_", slug)


def import_texture(name, normal):
    task = unreal.AssetImportTask()
    task.filename = os.path.join(ART, name + ".png")
    task.destination_path = DEST
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    TOOLS.import_asset_tasks([task])
    tex = unreal.load_asset(f"{DEST}/{name}")
    if normal:
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        tex.set_editor_property("srgb", False)
        tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD_NORMAL_MAP)
    else:
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
        tex.set_editor_property("srgb", False)  # data channels, not colour
    EAL.save_loaded_asset(tex)
    return tex


def expr(mat, cls, x, y):
    return MEL.create_material_expression(mat, cls, x, y)


def param(mat, cls, name, value, x, y):
    p = expr(mat, cls, x, y)
    p.set_editor_property("parameter_name", name)
    p.set_editor_property("default_value", value)
    return p


def mul(mat, a, b, x, y, a_out="", b_out=""):
    m = expr(mat, unreal.MaterialExpressionMultiply, x, y)
    MEL.connect_material_expressions(a, a_out, m, "A")
    MEL.connect_material_expressions(b, b_out, m, "B")
    return m


def add(mat, a, b, x, y, a_out="", b_out=""):
    m = expr(mat, unreal.MaterialExpressionAdd, x, y)
    MEL.connect_material_expressions(a, a_out, m, "A")
    MEL.connect_material_expressions(b, b_out, m, "B")
    return m


def build_hull_material(panels, normal):
    path = f"{DEST}/M_ShipHull"
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)
    mat = TOOLS.create_asset("M_ShipHull", DEST, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)

    uv = expr(mat, unreal.MaterialExpressionTextureCoordinate, -1600, 0)
    # Box UVs repeat every max(4, L/14) m; 0.35 makes a plate 2-9 m on a capital ship, readable at battle range.
    scale = param(mat, unreal.MaterialExpressionScalarParameter, "PanelScale", 0.35, -1600, 100)
    uvs = mul(mat, uv, scale, -1450, 40)
    tex = expr(mat, unreal.MaterialExpressionTextureSampleParameter2D, -1250, 0)
    tex.set_editor_property("parameter_name", "Panels")
    tex.set_editor_property("texture", panels)
    tex.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    MEL.connect_material_expressions(uvs, "", tex, "UVs")
    nrm = expr(mat, unreal.MaterialExpressionTextureSampleParameter2D, -1250, 400)
    nrm.set_editor_property("parameter_name", "Normal")
    nrm.set_editor_property("texture", normal)
    nrm.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    MEL.connect_material_expressions(uvs, "", nrm, "UVs")
    MEL.connect_material_property(nrm, "RGB", unreal.MaterialProperty.MP_NORMAL)
    vc = expr(mat, unreal.MaterialExpressionVertexColor, -1250, -400)

    # Base colour: hull or accent (vertex A), panel tone around 1.0 by PanelContrast, seams darkened.
    hull = param(mat, unreal.MaterialExpressionVectorParameter, "HullColor", C(0.5, 0.5, 0.52), -900, -300)
    accent = param(mat, unreal.MaterialExpressionVectorParameter, "AccentColor", C(0.3, 0.3, 0.35), -900, -200)
    lerp = expr(mat, unreal.MaterialExpressionLinearInterpolate, -700, -260)
    MEL.connect_material_expressions(hull, "", lerp, "A")
    MEL.connect_material_expressions(accent, "", lerp, "B")
    MEL.connect_material_expressions(vc, "A", lerp, "Alpha")
    contrast = param(mat, unreal.MaterialExpressionScalarParameter, "PanelContrast", 0.4, -900, -80)
    centred = expr(mat, unreal.MaterialExpressionSubtract, -900, 0)
    centred.set_editor_property("const_b", 0.5)
    MEL.connect_material_expressions(tex, "R", centred, "A")
    tone = mul(mat, centred, contrast, -760, -40)
    one_plus = expr(mat, unreal.MaterialExpressionAdd, -620, -40)
    one_plus.set_editor_property("const_b", 1.0)
    MEL.connect_material_expressions(tone, "", one_plus, "A")
    seam = expr(mat, unreal.MaterialExpressionOneMinus, -760, 60)
    seam_k = expr(mat, unreal.MaterialExpressionMultiply, -900, 80)
    seam_k.set_editor_property("const_b", 0.6)
    MEL.connect_material_expressions(tex, "G", seam_k, "A")
    MEL.connect_material_expressions(seam_k, "", seam, "")
    shade = mul(mat, one_plus, seam, -480, 0)
    base = mul(mat, lerp, shade, -300, -150)
    # Window bands read as dark glass between the lit cells.
    glass = expr(mat, unreal.MaterialExpressionLinearInterpolate, -150, -150)
    MEL.connect_material_expressions(base, "", glass, "A")
    dark = expr(mat, unreal.MaterialExpressionConstant3Vector, -300, -40)
    dark.set_editor_property("constant", C(0.03, 0.035, 0.045))
    MEL.connect_material_expressions(dark, "", glass, "B")
    band = mul(mat, vc, tex, -300, 60, "R", "B")
    band_k = expr(mat, unreal.MaterialExpressionMultiply, -200, 60)
    band_k.set_editor_property("const_b", 0.85)
    MEL.connect_material_expressions(band, "", band_k, "A")
    MEL.connect_material_expressions(band_k, "", glass, "Alpha")
    MEL.connect_material_property(glass, "", unreal.MaterialProperty.MP_BASE_COLOR)

    metallic = param(mat, unreal.MaterialExpressionScalarParameter, "Metallic", 0.5, -300, 180)
    MEL.connect_material_property(metallic, "", unreal.MaterialProperty.MP_METALLIC)
    rough_k = param(mat, unreal.MaterialExpressionScalarParameter, "RoughnessScale", 1.0, -480, 260)
    rough = mul(mat, tex, rough_k, -300, 260, "A", "")
    MEL.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)

    # Emissive: lit windows + warm glow + cool glow, each with its own colour and intensity.
    wcol = param(mat, unreal.MaterialExpressionVectorParameter, "WindowColor", C(1.0, 0.82, 0.55), -700, 400)
    wint = param(mat, unreal.MaterialExpressionScalarParameter, "WindowIntensity", 12.0, -700, 500)
    win = mul(mat, band, wcol, -480, 420)
    win = mul(mat, win, wint, -330, 420)
    gcol = param(mat, unreal.MaterialExpressionVectorParameter, "WarmColor", C(1.0, 0.3, 0.08), -700, 600)
    gint = param(mat, unreal.MaterialExpressionScalarParameter, "WarmIntensity", 40.0, -700, 700)
    warm = mul(mat, vc, gcol, -480, 620, "G", "")
    warm = mul(mat, warm, gint, -330, 620)
    ccol = param(mat, unreal.MaterialExpressionVectorParameter, "CoolColor", C(0.25, 0.55, 1.0), -700, 800)
    cint = param(mat, unreal.MaterialExpressionScalarParameter, "CoolIntensity", 40.0, -700, 900)
    cool = mul(mat, vc, ccol, -480, 820, "B", "")
    cool = mul(mat, cool, cint, -330, 820)
    emis = add(mat, add(mat, win, warm, -180, 520), cool, -40, 620)
    MEL.connect_material_property(emis, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.recompile_material(mat)
    EAL.save_loaded_asset(mat)
    return mat


def hull_instance(prefix, parent):
    name = "MI_Hull_" + ("generic" if prefix == "_" else prefix)
    path = f"{DEST}/{name}"
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)
    hull, accent, warm, cool, contrast, metallic = SCHEMES[prefix]
    mi = TOOLS.create_asset(name, DEST, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(mi, parent)
    MEL.set_material_instance_vector_parameter_value(mi, "HullColor", hull)
    MEL.set_material_instance_vector_parameter_value(mi, "AccentColor", accent)
    MEL.set_material_instance_vector_parameter_value(mi, "WarmColor", warm)
    MEL.set_material_instance_vector_parameter_value(mi, "CoolColor", cool)
    MEL.set_material_instance_scalar_parameter_value(mi, "PanelContrast", contrast)
    MEL.set_material_instance_scalar_parameter_value(mi, "Metallic", metallic)
    EAL.save_loaded_asset(mi)
    return mi


def mesh_options():
    opts = unreal.FbxImportUI()
    opts.import_mesh = True
    opts.import_as_skeletal = False
    opts.import_materials = False
    opts.import_textures = False
    opts.import_animations = False
    opts.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    sm = opts.static_mesh_import_data
    sm.combine_meshes = True
    sm.auto_generate_collision = False
    sm.generate_lightmap_u_vs = False
    sm.convert_scene = True
    sm.force_front_x_axis = False
    sm.import_uniform_scale = 1.0
    sm.vertex_color_import_option = unreal.VertexColorImportOption.REPLACE
    sm.normal_import_method = unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS
    return opts


def main():
    # Interchange (the 5.8 default for FBX) drops the "Mask" colour layer whatever FbxImportUI says; the legacy FBX
    # importer honours vertex_color_import_option. Scoped to this commandlet run.
    unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX 0")
    panels = import_texture("T_HullPanels", False)
    normal = import_texture("T_HullNormal", True)
    parent = build_hull_material(panels, normal)
    instances = {p: hull_instance(p, parent) for p in SCHEMES}
    ships = json.load(open(os.path.join(ART, "generated-ships.json"), encoding="utf-8"))["ships"]
    tasks = []
    for e in ships:
        fac, slug = e["key"].split(".")[1], e["key"].split(".", 2)[2]
        t = unreal.AssetImportTask()
        t.filename = os.path.join(ART, e["file"].replace("/", os.sep))
        t.destination_path = f"{DEST}/{fac}"
        t.destination_name = asset_name(slug)
        t.automated = True
        t.replace_existing = True
        t.save = False
        t.options = mesh_options()
        tasks.append((e, fac, t))
    # The StaticMeshEditorSubsystem is not available under the commandlet; the static library still is.
    sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem) or unreal.EditorStaticMeshLibrary
    ok = 0
    for e, fac, t in tasks:
        # One file per import call, saved straight away: a long batch held unsaved crashed the commandlet (5.8).
        unreal.log("BOTF ships: importing %s" % e["key"])
        TOOLS.import_asset_tasks([t])
        path = f"{DEST}/{fac}/{t.destination_name}"
        mesh = unreal.load_asset(path)
        if not isinstance(mesh, unreal.StaticMesh):
            unreal.log_warning("BOTF ships: %s did not import (%s)" % (e["key"], list(t.imported_object_paths)))
            continue
        mi = instances.get(fac, instances["_"])
        for i in range(max(1, len(mesh.static_materials))):
            mesh.set_material(i, mi)
        sockets = [n for n in e["sockets"] if mesh.find_socket(n)]
        colours = sub.has_vertex_colors(mesh)
        if not colours or len(sockets) < len(e["sockets"]):
            unreal.log_warning("BOTF ships: %s vertex colours=%s sockets %d/%d" % (e["key"], colours, len(sockets), len(e["sockets"])))
        EAL.save_loaded_asset(mesh)
        ok += 1
    unreal.log("BOTF_SHIPS_IMPORTED %d/%d" % (ok, len(ships)))


main()
