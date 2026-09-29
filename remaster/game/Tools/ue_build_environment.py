"""Unreal Editor Python: builds the battle scene's environment assets from ArtSource/Generated/Environment (original art).

    UnrealEditor-Cmd.exe BOTFRemake.uproject -run=pythonscript -script="<path>/ue_build_environment.py" -unattended -nosplash

Creates under /Game/Generated/Environment:
  T_SpaceNebula, T_GasGiant, T_RockyMoon   imported from ArtSource/Generated/Environment (generate_space_art.py)
  M_SkySphere   unlit two-sided sky: equirectangular lookup by view direction, so it has no UV seam or parallax.
                Parameters: Sky (texture), Brightness, Tint.
  M_Planet      lit body: equirectangular lookup by local direction, slow spin, Fresnel atmosphere rim.
                Parameters: Surface (texture), SpinSpeed, AtmosphereColor, AtmosphereStrength.
  MI_GasGiant, MI_RockyMoon   instances of M_Planet.
  SM_BodySphere a 256 x 128 UV sphere (radius 50) for the planet, moon and sky, written here as OBJ: the engine spheres
                show facets on the limb of a gas giant that fills a third of the frame.
Idempotent: existing assets are replaced.
"""
import math
import os

import unreal

PROJECT_DIR = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
ART = os.path.join(PROJECT_DIR, "ArtSource", "Generated", "Environment")
DEST = "/Game/Generated/Environment"
MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()

LATLONG = """float3 d = normalize(Dir);
float u = atan2(d.y, d.x) * 0.15915494 + 0.5 + Offset;
float v = acos(clamp(d.z, -1.0, 1.0)) * 0.31830989;
return float2(u, v);"""


def import_texture(name):
    task = unreal.AssetImportTask()
    task.filename = os.path.join(ART, name + ".png")
    task.destination_path = DEST
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    TOOLS.import_asset_tasks([task])
    tex = unreal.load_asset(f"{DEST}/{name}")
    # Equirectangular lookups wrap at u = 0/1; without mips the wrap has no derivative seam.
    tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_SKYBOX)
    tex.set_editor_property("never_stream", True)
    tex.set_editor_property("address_x", unreal.TextureAddress.TA_WRAP)
    tex.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
    EAL.save_loaded_asset(tex)
    return tex


def new_material(name):
    path = f"{DEST}/{name}"
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)
    return TOOLS.create_asset(name, DEST, unreal.Material, unreal.MaterialFactoryNew())


def expr(mat, cls, x, y):
    return MEL.create_material_expression(mat, cls, x, y)


def latlong_node(mat, x, y):
    custom = expr(mat, unreal.MaterialExpressionCustom, x, y)
    custom.set_editor_property("code", LATLONG)
    custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT2)
    custom.set_editor_property("description", "LatLong")
    inputs = []
    for n in ("Dir", "Offset"):
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", n)
        inputs.append(ci)
    custom.set_editor_property("inputs", inputs)
    return custom


def build_sky(tex):
    mat = new_material("M_SkySphere")
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("two_sided", True)
    cam = expr(mat, unreal.MaterialExpressionCameraVectorWS, -900, 0)
    neg = expr(mat, unreal.MaterialExpressionMultiply, -760, 0)
    neg.set_editor_property("const_b", -1.0)
    MEL.connect_material_expressions(cam, "", neg, "A")
    zero = expr(mat, unreal.MaterialExpressionConstant, -760, 120)
    ll = latlong_node(mat, -600, 0)
    MEL.connect_material_expressions(neg, "", ll, "Dir")
    MEL.connect_material_expressions(zero, "", ll, "Offset")
    sample = expr(mat, unreal.MaterialExpressionTextureSampleParameter2D, -380, 0)
    sample.set_editor_property("parameter_name", "Sky")
    sample.set_editor_property("texture", tex)
    sample.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    MEL.connect_material_expressions(ll, "", sample, "UVs")
    bright = expr(mat, unreal.MaterialExpressionScalarParameter, -380, 220)
    bright.set_editor_property("parameter_name", "Brightness")
    bright.set_editor_property("default_value", 1.0)
    tint = expr(mat, unreal.MaterialExpressionVectorParameter, -380, 320)
    tint.set_editor_property("parameter_name", "Tint")
    tint.set_editor_property("default_value", unreal.LinearColor(1, 1, 1, 1))
    m1 = expr(mat, unreal.MaterialExpressionMultiply, -150, 0)
    MEL.connect_material_expressions(sample, "RGB", m1, "A")
    MEL.connect_material_expressions(bright, "", m1, "B")
    m2 = expr(mat, unreal.MaterialExpressionMultiply, -20, 0)
    MEL.connect_material_expressions(m1, "", m2, "A")
    MEL.connect_material_expressions(tint, "", m2, "B")
    MEL.connect_material_property(m2, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.recompile_material(mat)
    EAL.save_loaded_asset(mat)
    return mat


def build_planet(default_tex):
    mat = new_material("M_Planet")
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    local = expr(mat, unreal.MaterialExpressionLocalPosition, -1000, 0)
    time = expr(mat, unreal.MaterialExpressionTime, -1000, 140)
    spin = expr(mat, unreal.MaterialExpressionScalarParameter, -1000, 220)
    spin.set_editor_property("parameter_name", "SpinSpeed")
    spin.set_editor_property("default_value", 0.004)
    offset = expr(mat, unreal.MaterialExpressionMultiply, -820, 160)
    MEL.connect_material_expressions(time, "", offset, "A")
    MEL.connect_material_expressions(spin, "", offset, "B")
    ll = latlong_node(mat, -640, 0)
    MEL.connect_material_expressions(local, "", ll, "Dir")
    MEL.connect_material_expressions(offset, "", ll, "Offset")
    sample = expr(mat, unreal.MaterialExpressionTextureSampleParameter2D, -420, 0)
    sample.set_editor_property("parameter_name", "Surface")
    sample.set_editor_property("texture", default_tex)
    MEL.connect_material_expressions(ll, "", sample, "UVs")
    MEL.connect_material_property(sample, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = expr(mat, unreal.MaterialExpressionConstant, -200, 200)
    rough.set_editor_property("r", 0.85)
    MEL.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    spec = expr(mat, unreal.MaterialExpressionConstant, -200, 260)
    spec.set_editor_property("r", 0.25)
    MEL.connect_material_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)
    fres = expr(mat, unreal.MaterialExpressionFresnel, -420, 320)
    fres.set_editor_property("exponent", 3.5)
    fres.set_editor_property("base_reflect_fraction", 0.0)
    colour = expr(mat, unreal.MaterialExpressionVectorParameter, -420, 440)
    colour.set_editor_property("parameter_name", "AtmosphereColor")
    colour.set_editor_property("default_value", unreal.LinearColor(0.9, 0.6, 0.35, 1))
    strength = expr(mat, unreal.MaterialExpressionScalarParameter, -420, 560)
    strength.set_editor_property("parameter_name", "AtmosphereStrength")
    strength.set_editor_property("default_value", 1.5)
    a = expr(mat, unreal.MaterialExpressionMultiply, -200, 360)
    MEL.connect_material_expressions(fres, "", a, "A")
    MEL.connect_material_expressions(colour, "", a, "B")
    b = expr(mat, unreal.MaterialExpressionMultiply, -60, 360)
    MEL.connect_material_expressions(a, "", b, "A")
    MEL.connect_material_expressions(strength, "", b, "B")
    MEL.connect_material_property(b, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.recompile_material(mat)
    EAL.save_loaded_asset(mat)
    return mat


def planet_instance(name, parent, tex, atmosphere, strength):
    path = f"{DEST}/{name}"
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)
    mi = TOOLS.create_asset(name, DEST, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(mi, parent)
    MEL.set_material_instance_texture_parameter_value(mi, "Surface", tex)
    MEL.set_material_instance_vector_parameter_value(mi, "AtmosphereColor", atmosphere)
    MEL.set_material_instance_scalar_parameter_value(mi, "AtmosphereStrength", strength)
    EAL.save_loaded_asset(mi)


def write_sphere(path, seg=256, rings=128, radius=50.0):
    """UV sphere with smooth normals. The materials map by local direction, so its UVs and axes do not matter."""
    lines = []
    for r in range(rings + 1):
        th = math.pi * r / rings
        for s in range(seg + 1):
            ph = 2 * math.pi * s / seg
            n = (math.sin(th) * math.cos(ph), math.sin(th) * math.sin(ph), math.cos(th))
            lines.append("v %.5f %.5f %.5f" % (n[0] * radius, n[1] * radius, n[2] * radius))
            lines.append("vn %.5f %.5f %.5f" % n)
            lines.append("vt %.5f %.5f" % (s / seg, 1 - r / rings))
    row = seg + 1
    for r in range(rings):
        for s in range(seg):
            a, b = r * row + s + 1, r * row + s + 2
            c, d = a + row, b + row
            if r > 0:
                lines.append("f %d/%d/%d %d/%d/%d %d/%d/%d" % (a, a, a, c, c, c, b, b, b))
            if r < rings - 1:
                lines.append("f %d/%d/%d %d/%d/%d %d/%d/%d" % (b, b, b, c, c, c, d, d, d))
    with open(path, "w") as f:
        f.write("\n".join(lines) + "\n")


def import_sphere():
    path = os.path.join(ART, "SM_BodySphere.obj")
    write_sphere(path)
    task = unreal.AssetImportTask()
    task.filename = path
    task.destination_path = DEST
    task.destination_name = "SM_BodySphere"
    task.automated = True
    task.replace_existing = True
    task.save = True
    TOOLS.import_asset_tasks([task])
    mesh = unreal.load_asset(f"{DEST}/SM_BodySphere")
    if not mesh:
        unreal.log_error("Missing SM_BodySphere after import")
        return
    # OBJ imports default to Nanite; the battle scene capture draws Nanite meshes by their coarse fallback, which put
    # facets back on the limb. A plain 65k-triangle mesh is cheap for one planet.
    nanite = mesh.get_editor_property("nanite_settings")
    nanite.set_editor_property("enabled", False)
    mesh.set_editor_property("nanite_settings", nanite)
    EAL.save_loaded_asset(mesh)


def main():
    import_sphere()
    sky = import_texture("T_SpaceNebula")
    giant = import_texture("T_GasGiant")
    moon = import_texture("T_RockyMoon")
    build_sky(sky)
    planet = build_planet(giant)
    planet_instance("MI_GasGiant", planet, giant, unreal.LinearColor(0.95, 0.62, 0.35, 1), 1.6)
    planet_instance("MI_RockyMoon", planet, moon, unreal.LinearColor(0.5, 0.6, 0.8, 1), 0.35)
    unreal.log("BOTF_ENVIRONMENT_BUILT")


main()
