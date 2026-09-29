"""Unreal Editor Python: imports the hero ships built by blender_hero_ships.py into /Game/Local (gitignored).

    UnrealEditor-Cmd.exe BOTFRemake.uproject -run=pythonscript -script="<path>/ue_import_hero_ships.py" -unattended -nosplash -nullrhi

Inputs: LocalAssets/ships/<key>/hero.json, SM_<slug>.fbx and T_<fac>_<slug>_<slot>_{BC,N,ORM,E}.png.
Creates:
  /Game/Local/Ships/M_ShipHero                 textured hull: BC x Tint, tangent normal, ORM (occlusion, roughness,
                                                metallic), emissive windows/engines x EmissiveStrength
  /Game/Local/Ships/<fac>/<slug>/SM_<slug>     sockets kept, LODs 0-3 by screen size, Nanite off (scene captures
                                                render the Nanite fallback mesh)
  /Game/Local/Ships/<fac>/<slug>/MI_<slug>_<slot>
Untextured slots marked "plating" use /Game/Generated/Ships/MI_Hull_<fac> (run ue_import_generated_ships.py first).
Writes LocalAssets/ships/hero-manifest.json and merges it into LocalAssets/resolved-manifest.json, so
UBotfAssetRegistry resolves those keys to the hero meshes. Import-Assets.ps1 re-applies hero-manifest.json.
"""
import glob
import json
import os

import unreal

PROJECT_DIR = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SHIPS = os.path.join(PROJECT_DIR, "LocalAssets", "ships")
RESOLVED = os.path.join(PROJECT_DIR, "LocalAssets", "resolved-manifest.json")
DEST = "/Game/Local/Ships"
MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
C = unreal.LinearColor


def expr(mat, cls, x, y):
    return MEL.create_material_expression(mat, cls, x, y)


def param(mat, cls, name, value, x, y):
    p = expr(mat, cls, x, y)
    p.set_editor_property("parameter_name", name)
    p.set_editor_property("default_value", value)
    return p


def sampler(mat, name, texture, kind, x, y):
    s = expr(mat, unreal.MaterialExpressionTextureSampleParameter2D, x, y)
    s.set_editor_property("parameter_name", name)
    s.set_editor_property("texture", unreal.load_asset(texture))
    s.set_editor_property("sampler_type", kind)
    return s


def mul(mat, a, b, x, y, a_out="", b_out=""):
    m = expr(mat, unreal.MaterialExpressionMultiply, x, y)
    MEL.connect_material_expressions(a, a_out, m, "A")
    MEL.connect_material_expressions(b, b_out, m, "B")
    return m


def solid_png(path, rgb):
    """A 4x4 solid PNG (sampler defaults whose compression matches the sampler type)."""
    import struct
    import zlib
    raw = b"".join(b"\x00" + bytes(rgb) * 4 for _ in range(4))

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", 4, 4, 8, 2, 0, 0, 0))
                + chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b""))


def defaults():
    out = {}
    for name, rgb, kind in (("T_HeroDefault_BC", (255, 255, 255), "bc"), ("T_HeroDefault_ORM", (255, 255, 255), "orm"),
                            ("T_HeroDefault_E", (0, 0, 0), "e")):
        path = os.path.join(SHIPS, name + ".png")
        solid_png(path, rgb)
        out[kind] = import_texture(path, DEST, kind).get_path_name()
    return out


def build_master():
    path = f"{DEST}/M_ShipHero"
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)
    base = defaults()
    mat = TOOLS.create_asset("M_ShipHero", DEST, unreal.Material, unreal.MaterialFactoryNew())
    ST = unreal.MaterialSamplerType
    bc = sampler(mat, "BaseColor", base["bc"], ST.SAMPLERTYPE_COLOR, -900, -300)
    tint = param(mat, unreal.MaterialExpressionVectorParameter, "Tint", C(1, 1, 1), -900, -120)
    MEL.connect_material_property(mul(mat, bc, tint, -600, -250, "RGB", ""), "", unreal.MaterialProperty.MP_BASE_COLOR)
    nrm = sampler(mat, "Normal", "/Engine/EngineMaterials/DefaultNormal", ST.SAMPLERTYPE_NORMAL, -900, 60)
    MEL.connect_material_property(nrm, "RGB", unreal.MaterialProperty.MP_NORMAL)
    orm = sampler(mat, "ORM", base["orm"], ST.SAMPLERTYPE_MASKS, -900, 300)
    rough = param(mat, unreal.MaterialExpressionScalarParameter, "RoughnessScale", 1.0, -900, 480)
    metal = param(mat, unreal.MaterialExpressionScalarParameter, "MetallicScale", 1.0, -900, 560)
    MEL.connect_material_property(orm, "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    MEL.connect_material_property(mul(mat, orm, rough, -600, 380, "G", ""), "", unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.connect_material_property(mul(mat, orm, metal, -600, 480, "B", ""), "", unreal.MaterialProperty.MP_METALLIC)
    emi = sampler(mat, "Emissive", base["e"], ST.SAMPLERTYPE_COLOR, -900, 700)
    k = param(mat, unreal.MaterialExpressionScalarParameter, "EmissiveStrength", 1.5, -900, 880)
    MEL.connect_material_property(mul(mat, emi, k, -600, 760, "RGB", ""), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.recompile_material(mat)
    EAL.save_loaded_asset(mat)
    return mat


def import_texture(src, dest, kind):
    t = unreal.AssetImportTask()
    t.filename = src
    t.destination_path = dest
    t.automated = True
    t.replace_existing = True
    t.save = False
    TOOLS.import_asset_tasks([t])
    tex = unreal.load_asset(f"{dest}/{os.path.splitext(os.path.basename(src))[0]}")
    if kind == "n":
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        tex.set_editor_property("srgb", False)
        tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD_NORMAL_MAP)
    elif kind == "orm":
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
        tex.set_editor_property("srgb", False)
    else:
        tex.set_editor_property("srgb", True)
    EAL.save_loaded_asset(tex)
    return tex


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


def set_lods(mesh):
    options = unreal.EditorScriptingMeshReductionOptions()
    settings = []
    for percent, screen in ((1.0, 1.0), (0.5, 0.35), (0.25, 0.15), (0.1, 0.05)):
        s = unreal.EditorScriptingMeshReductionSettings()
        s.percent_triangles = percent
        s.screen_size = screen
        settings.append(s)
    options.reduction_settings = settings
    options.auto_compute_lod_screen_size = False
    unreal.EditorStaticMeshLibrary.set_lods(mesh, options)


def import_ship(folder, master):
    info = json.load(open(os.path.join(folder, "hero.json"), encoding="utf-8"))
    key = info["key"]
    fac, slug = key.split(".")[1], key.split(".", 2)[2]
    dest = f"{DEST}/{fac}/{slug}"
    t = unreal.AssetImportTask()
    t.filename = os.path.join(folder, info["file"])
    t.destination_path = dest
    t.destination_name = "SM_" + slug
    t.automated = True
    t.replace_existing = True
    t.save = False
    t.options = mesh_options()
    TOOLS.import_asset_tasks([t])
    mesh_path = f"{dest}/SM_{slug}"
    mesh = unreal.load_asset(mesh_path)
    if not isinstance(mesh, unreal.StaticMesh):
        unreal.log_warning("BOTF heroes: %s did not import (%s)" % (key, list(t.imported_object_paths)))
        return None
    nanite = mesh.get_editor_property("nanite_settings")
    nanite.enabled = False
    mesh.set_editor_property("nanite_settings", nanite)
    slot_names = [str(m.material_slot_name) for m in mesh.static_materials]
    for slot in info["slots"]:
        index = slot_names.index(slot["name"]) if slot["name"] in slot_names else (0 if len(slot_names) == 1 else -1)
        if index < 0:
            unreal.log_warning("BOTF heroes: %s has no slot %s (%s)" % (key, slot["name"], slot_names))
            continue
        if slot.get("plating"):
            mi = unreal.load_asset("/Game/Generated/Ships/MI_Hull_%s" % slot["plating"])
            if mi:
                mesh.set_material(index, mi)
            continue
        name = "MI_%s_%s" % (slug, slot["name"])
        if EAL.does_asset_exist(f"{dest}/{name}"):
            EAL.delete_asset(f"{dest}/{name}")
        mi = TOOLS.create_asset(name, dest, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        MEL.set_material_instance_parent(mi, master)
        for kind, pname in (("bc", "BaseColor"), ("n", "Normal"), ("orm", "ORM"), ("e", "Emissive")):
            if slot.get(kind):
                tex = import_texture(os.path.join(folder, slot[kind]), dest, kind)
                MEL.set_material_instance_texture_parameter_value(mi, pname, tex)
        if not slot.get("bc") and slot.get("tint"):
            MEL.set_material_instance_vector_parameter_value(mi, "Tint", C(*slot["tint"], 1.0))
        # Starship hulls are painted plating, mostly dielectric: the battle capture has no reflection environment, so a
        # strongly metallic hull reflects nothing and renders near black. A third of the baked metalness keeps a sheen.
        if not slot.get("orm"):
            MEL.set_material_instance_scalar_parameter_value(mi, "RoughnessScale", 0.45)
            MEL.set_material_instance_scalar_parameter_value(mi, "MetallicScale", 0.35 * slot.get("metallic", 0.5))
        else:
            MEL.set_material_instance_scalar_parameter_value(mi, "MetallicScale", 0.35)
        if slot.get("e"):
            MEL.set_material_instance_scalar_parameter_value(mi, "EmissiveStrength", 1.5 * max(1.0, slot.get("emissive", 1.0)))
        EAL.save_loaded_asset(mi)
        mesh.set_material(index, mi)
    set_lods(mesh)
    sockets = [s for s in info["sockets"] if mesh.find_socket(s)]
    if len(sockets) < len(info["sockets"]):
        unreal.log_warning("BOTF heroes: %s sockets %d/%d" % (key, len(sockets), len(info["sockets"])))
    EAL.save_loaded_asset(mesh)
    return key, {"kind": "model", "status": "resolved", "source": "hero: " + info["source"], "faction": fac,
                 "targetLengthMeters": info["lengthMeters"], "ueFolder": dest, "ueAssets": [f"{mesh_path}.SM_{slug}"],
                 "importFiles": [t.filename], "effectiveKey": key}


def main():
    master = build_master()
    entries = {}
    folders = sorted(os.path.dirname(p) for p in glob.glob(os.path.join(SHIPS, "*", "hero.json")))
    for folder in folders:
        unreal.log("BOTF heroes: importing %s" % os.path.basename(folder))
        r = import_ship(folder, master)
        if r:
            entries[r[0]] = r[1]
    with open(os.path.join(SHIPS, "hero-manifest.json"), "w", encoding="utf-8") as f:
        json.dump({"assets": entries}, f, indent=1)
    resolved = json.load(open(RESOLVED, encoding="utf-8-sig")) if os.path.exists(RESOLVED) else {"assets": {}}
    for key, entry in entries.items():
        old = resolved["assets"].get(key, {})
        resolved["assets"][key] = {**old, **entry, "fallback": old.get("fallback", entry.get("fallback"))}
    with open(RESOLVED, "w", encoding="utf-8") as f:
        json.dump(resolved, f, indent=1)
    unreal.log_warning("BOTF_HEROES_IMPORTED %d/%d" % (len(entries), len(folders)))


main()
