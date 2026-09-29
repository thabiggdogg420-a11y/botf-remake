"""Unreal Editor Python: builds the battle effect materials in /Game/Generated/VFX (original art).

    UnrealEditor-Cmd.exe BOTFRemake.uproject -run=pythonscript -script="<path>/ue_build_vfx.py" -unattended -nosplash -nullrhi

The rules 3 timeline draws its fallback effects on pooled engine meshes (BotfCombatScene.cpp); these materials make
those meshes read as light rather than as solid shapes. Niagara systems under vfx.* keys still take precedence.
  T_FxNoise     imported from ArtSource/Generated/VFX (generate_fx_textures.py)
  M_FxGlow      additive unlit. Brightness falls off from the facing centre to the silhouette, and the core burns
                toward white. A sphere reads as a soft glow and a cylinder as a beam with a hot core.
                Parameters: Color, Intensity, Falloff, CoreWhite.
  M_FxFireball  additive unlit. Two panning noise layers eroded by Age (0 to 1) over a temperature ramp: white-yellow
                core, orange body, dark red smoke edge, burning out as Age rises.
                Parameters: Color, Age, Intensity.
  MI_FxDebris   dark hull plating with glowing edges (an instance of /Game/Generated/Ships/M_ShipHull when present).
Idempotent: existing assets are replaced.
"""
import os

import unreal

PROJECT_DIR = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
ART = os.path.join(PROJECT_DIR, "ArtSource", "Generated", "VFX")
DEST = "/Game/Generated/VFX"
MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
C = unreal.LinearColor


def import_noise():
    task = unreal.AssetImportTask()
    task.filename = os.path.join(ART, "T_FxNoise.png")
    task.destination_path = DEST
    task.destination_name = "T_FxNoise"
    task.automated = True
    task.replace_existing = True
    task.save = False
    TOOLS.import_asset_tasks([task])
    tex = unreal.load_asset(f"{DEST}/T_FxNoise")
    tex.set_editor_property("srgb", False)
    tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
    EAL.save_loaded_asset(tex)
    return tex


def new_material(name):
    path = f"{DEST}/{name}"
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)
    mat = TOOLS.create_asset(name, DEST, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    mat.set_editor_property("used_with_static_lighting", False)
    return mat


def e(mat, cls, x, y, **props):
    node = MEL.create_material_expression(mat, cls, x, y)
    for k, v in props.items():
        node.set_editor_property(k, v)
    return node


def link(a, b, pin="", out=""):
    MEL.connect_material_expressions(a, out, b, pin)


def binop(mat, cls, a, b, x, y, a_out="", b_out=""):
    node = e(mat, cls, x, y)
    pins = ("Base", "Exponent") if cls == unreal.MaterialExpressionPower else ("A", "B")
    link(a, node, pins[0], a_out)
    link(b, node, pins[1], b_out)
    return node


def scalar(mat, name, value, x, y):
    return e(mat, unreal.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=value)


def vector(mat, name, value, x, y):
    return e(mat, unreal.MaterialExpressionVectorParameter, x, y, parameter_name=name, default_value=value)


def facing(mat, x, y):
    """saturate(dot(normal, camera)): 1 at the centre of a sphere as seen, 0 at its silhouette."""
    n = e(mat, unreal.MaterialExpressionVertexNormalWS, x, y)
    c = e(mat, unreal.MaterialExpressionCameraVectorWS, x, y + 100)
    d = binop(mat, unreal.MaterialExpressionDotProduct, n, c, x + 160, y + 40)
    s = e(mat, unreal.MaterialExpressionSaturate, x + 300, y + 40)
    link(d, s)
    return s


def build_glow():
    mat = new_material("M_FxGlow")
    f = facing(mat, -1200, 0)
    falloff = scalar(mat, "Falloff", 2.2, -1000, 160)
    p = binop(mat, unreal.MaterialExpressionPower, f, falloff, -800, 40)
    colour = vector(mat, "Color", C(1, 0.6, 0.3, 1), -1000, -200)
    core_k = scalar(mat, "CoreWhite", 0.75, -1000, 260)
    core = binop(mat, unreal.MaterialExpressionPower, f, e(mat, unreal.MaterialExpressionConstant, -1000, 360, r=8.0), -800, 300)
    core = binop(mat, unreal.MaterialExpressionMultiply, core, core_k, -650, 300)
    white = e(mat, unreal.MaterialExpressionConstant3Vector, -800, -120, constant=C(1, 1, 1, 1))
    tint = e(mat, unreal.MaterialExpressionLinearInterpolate, -500, -150)
    link(colour, tint, "A")
    link(white, tint, "B")
    link(core, tint, "Alpha")
    intensity = scalar(mat, "Intensity", 5.0, -500, 120)
    m1 = binop(mat, unreal.MaterialExpressionMultiply, tint, p, -300, -60)
    m2 = binop(mat, unreal.MaterialExpressionMultiply, m1, intensity, -150, -60)
    # Colour alpha scales the whole glow (the scene fades orbs by scaling Color).
    m3 = binop(mat, unreal.MaterialExpressionMultiply, m2, colour, 0, -60, "", "A")
    MEL.connect_material_property(m3, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.recompile_material(mat)
    EAL.save_loaded_asset(mat)


def noise_layer(mat, tex, scale, speed_u, speed_v, x, y):
    uv = e(mat, unreal.MaterialExpressionTextureCoordinate, x, y, u_tiling=scale, v_tiling=scale)
    pan = e(mat, unreal.MaterialExpressionPanner, x + 160, y, speed_x=speed_u, speed_y=speed_v)
    link(uv, pan, "Coordinate")
    s = e(mat, unreal.MaterialExpressionTextureSample, x + 320, y, texture=tex,
          sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    link(pan, s, "UVs")
    return s


def build_fireball(tex):
    mat = new_material("M_FxFireball")
    mat.set_editor_property("two_sided", False)
    a = noise_layer(mat, tex, 1.0, 0.05, -0.09, -1500, -200)
    b = noise_layer(mat, tex, 2.3, -0.07, 0.04, -1500, 100)
    body = binop(mat, unreal.MaterialExpressionMultiply, a, b, -1050, -50, "R", "G")
    body = binop(mat, unreal.MaterialExpressionMultiply, body, e(mat, unreal.MaterialExpressionConstant, -1200, 60, r=2.4), -900, -50)
    f = facing(mat, -1500, 400)
    fp = binop(mat, unreal.MaterialExpressionPower, f, e(mat, unreal.MaterialExpressionConstant, -1200, 520, r=0.8), -1000, 420)
    heat = binop(mat, unreal.MaterialExpressionMultiply, body, fp, -750, 100)
    age = scalar(mat, "Age", 0.0, -900, 300)
    erode = binop(mat, unreal.MaterialExpressionMultiply, age, e(mat, unreal.MaterialExpressionConstant, -900, 380, r=1.15), -750, 300)
    heat = binop(mat, unreal.MaterialExpressionSubtract, heat, erode, -600, 150)
    heat_s = e(mat, unreal.MaterialExpressionSaturate, -460, 150)
    link(heat, heat_s)
    # Temperature ramp: dark red smoke -> orange -> yellow-white core.
    ramp1 = e(mat, unreal.MaterialExpressionLinearInterpolate, -300, -100)
    link(e(mat, unreal.MaterialExpressionConstant3Vector, -460, -260, constant=C(0.35, 0.04, 0.01, 1)), ramp1, "A")
    link(e(mat, unreal.MaterialExpressionConstant3Vector, -460, -160, constant=C(1.0, 0.42, 0.08, 1)), ramp1, "B")
    t1 = binop(mat, unreal.MaterialExpressionMultiply, heat_s, e(mat, unreal.MaterialExpressionConstant, -460, 40, r=2.5), -380, 0)
    t1s = e(mat, unreal.MaterialExpressionSaturate, -300, 0)
    link(t1, t1s)
    link(t1s, ramp1, "Alpha")
    ramp2 = e(mat, unreal.MaterialExpressionLinearInterpolate, -150, -60)
    link(ramp1, ramp2, "A")
    link(e(mat, unreal.MaterialExpressionConstant3Vector, -300, 80, constant=C(1.0, 0.9, 0.62, 1)), ramp2, "B")
    t2 = binop(mat, unreal.MaterialExpressionSubtract, heat_s, e(mat, unreal.MaterialExpressionConstant, -300, 200, r=0.45), -220, 160)
    t2 = binop(mat, unreal.MaterialExpressionMultiply, t2, e(mat, unreal.MaterialExpressionConstant, -300, 260, r=2.2), -150, 160)
    t2s = e(mat, unreal.MaterialExpressionSaturate, -80, 160)
    link(t2, t2s)
    link(t2s, ramp2, "Alpha")
    colour = vector(mat, "Color", C(1, 1, 1, 1), -150, 280)
    tinted = binop(mat, unreal.MaterialExpressionMultiply, ramp2, colour, 0, -20)
    intensity = scalar(mat, "Intensity", 9.0, 0, 200)
    lit = binop(mat, unreal.MaterialExpressionMultiply, tinted, heat_s, 150, 0)
    lit = binop(mat, unreal.MaterialExpressionMultiply, lit, intensity, 300, 0)
    MEL.connect_material_property(lit, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.recompile_material(mat)
    EAL.save_loaded_asset(mat)


def build_debris():
    parent = unreal.load_asset("/Game/Generated/Ships/M_ShipHull")
    if not parent:
        unreal.log_warning("BOTF vfx: M_ShipHull missing, MI_FxDebris skipped (run ue_import_generated_ships.py first)")
        return
    path = f"{DEST}/MI_FxDebris"
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)
    mi = TOOLS.create_asset("MI_FxDebris", DEST, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(mi, parent)
    MEL.set_material_instance_vector_parameter_value(mi, "HullColor", C(0.12, 0.11, 0.1, 1))
    MEL.set_material_instance_vector_parameter_value(mi, "AccentColor", C(0.12, 0.11, 0.1, 1))
    MEL.set_material_instance_scalar_parameter_value(mi, "PanelScale", 2.0)
    MEL.set_material_instance_scalar_parameter_value(mi, "PanelContrast", 0.8)
    EAL.save_loaded_asset(mi)


def main():
    tex = import_noise()
    build_glow()
    build_fireball(tex)
    build_debris()
    unreal.log_warning("BOTF_VFX_BUILT")


main()
