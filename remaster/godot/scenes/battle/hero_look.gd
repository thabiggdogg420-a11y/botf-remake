class_name HeroLook
extends RefCounted
## Look of a local hero model (blender_hero_ships.py --gltf, listed in LocalAssets/ships/godot-hero-manifest.json):
## the Unreal importer's materials rebuilt from the manifest's slot hints, and the glb's LOD meshes switched by
## distance. See remaster/game/Tools/hero_gltf_manifest.py for the hint fields.
##   textured slot -> hero_hull.gdshader   (M_ShipHero: BC x tint, normal, ORM x scales, roughness floor, emissive)
##   plated slot   -> hero_plated.gdshader (M_ShipPlated: vertex colour "Col" x tiling faction plating)

const HULL_SHADER := preload("res://scenes/battle/shaders/hero_hull.gdshader")
const PLATED_SHADER := preload("res://scenes/battle/shaders/hero_plated.gdshader")
const CAMERA_FOV := 48.0   ## BattleCamera's fov: LOD distances follow Unreal's screen sizes through it.

static var _textures := {}  ## absolute path -> Texture2D (or null when it failed to load)


## Replaces every surface material under `hull` by its hero material. Returns the number of surfaces set.
static func apply(hull: Node, hero: Dictionary, root_dir: String) -> int:
	var slots: Array = hero.get("slots", [])
	var count := 0
	var cache := {}
	var meshes: Array = hull.find_children("*", "MeshInstance3D", true, false)
	if hull is MeshInstance3D:
		meshes.append(hull)
	for mi: MeshInstance3D in meshes:
		if mi.mesh == null:
			continue
		for i in mi.mesh.get_surface_count():
			var src: Material = mi.get_active_material(i)
			var slot := slot_for(slots, src.resource_name if src != null else "", i)
			if slot.is_empty():
				continue
			var key := "%s|%s" % [slot.get("name", ""), src.get_instance_id() if src != null else 0]
			if not cache.has(key):
				cache[key] = material_for(slot, src, hero, root_dir)
			mi.set_surface_override_material(i, cache[key])
			count += 1
	return count


## The hint for a surface: by material name (M00, M01 ... as Blender named the slots), else the only slot, else the
## slot at the surface index.
static func slot_for(slots: Array, material_name: String, surface: int) -> Dictionary:
	for s in slots:
		if str(s.get("name", "")) == material_name and material_name != "":
			return s
	if slots.size() == 1:
		return slots[0]
	if surface < slots.size():
		return slots[surface]
	return {}


## A glb loaded at runtime brings its images without mipmaps, so a hull seen from afar sampled the full-size texture
## and its window dots and panel lines aliased into black speckle (R3.5, title close-ups). Builds the mip chain once
## per texture (cached), compressed-format images are left as they are.
static var _mipped := {}
static func mipmapped(t: Texture2D) -> Texture2D:
	if t == null:
		return null
	var id := t.get_instance_id()
	if _mipped.has(id):
		return _mipped[id]
	var out: Texture2D = t
	var img := t.get_image()
	if img != null and not img.has_mipmaps() and not img.is_compressed():
		img.generate_mipmaps()
		out = ImageTexture.create_from_image(img)
	_mipped[id] = out
	return out


static func material_for(slot: Dictionary, src: Material, hero: Dictionary, root_dir: String) -> ShaderMaterial:
	var m := ShaderMaterial.new()
	var std := src as BaseMaterial3D
	if str(slot.get("kind", "textured")) == "plated":
		m.shader = PLATED_SHADER
		m.resource_name = "HeroPlated_%s" % slot.get("name", "")
		var vc = slot.get("vertexColor")
		m.set_shader_parameter("use_vertex_color", vc != null and str(vc) != "")
		var tint = slot.get("tint")
		if tint is Array and tint.size() >= 3:
			m.set_shader_parameter("tint", Vector3(tint[0], tint[1], tint[2]))
		var tone: Array = slot.get("tone", [0.7, 0.65])
		m.set_shader_parameter("tone", Vector2(tone[0], tone[1]))
		m.set_shader_parameter("seam", float(slot.get("seam", 0.55)))
		m.set_shader_parameter("panel_scale", float(slot.get("panelScale", 0.35)))
		m.set_shader_parameter("roughness_min", float(slot.get("roughnessMin", 0.32)))
		m.set_shader_parameter("metallic", float(slot.get("metallic", 0.15)))
		var plating: Dictionary = hero.get("plating", {})
		var panels := _texture(root_dir, str(plating.get("panels", "")))
		var normal := _texture(root_dir, str(plating.get("normal", "")))
		m.set_shader_parameter("has_panels", panels != null)
		if panels != null:
			m.set_shader_parameter("panels_tex", panels)
		m.set_shader_parameter("has_panel_normal", normal != null)
		if normal != null:
			m.set_shader_parameter("panel_normal_tex", normal)
		return m
	m.shader = HULL_SHADER
	m.resource_name = "HeroHull_%s" % slot.get("name", "")
	var tint = slot.get("tint")
	if tint is Array and tint.size() >= 3:
		m.set_shader_parameter("tint", Vector3(tint[0], tint[1], tint[2]))
	m.set_shader_parameter("metallic_scale", float(slot.get("metallicScale", 0.35)))
	m.set_shader_parameter("roughness_scale", float(slot.get("roughnessScale", 1.0)))
	m.set_shader_parameter("roughness_min", float(slot.get("roughnessMin", 0.32)))
	m.set_shader_parameter("emissive_strength", float(slot.get("emissiveStrength", 0.0)))
	if std != null:
		# Godot's glTF import puts BC in albedo, the normal map in normal, metallicRoughness (our ORM image: G
		# roughness, B metallic, and R occlusion) in roughness / metallic, occlusion in ao, and E in emission.
		if std.albedo_texture != null:
			m.set_shader_parameter("bc_tex", HeroLook.mipmapped(std.albedo_texture))
		elif tint == null:
			var c := std.albedo_color.srgb_to_linear()
			m.set_shader_parameter("tint", Vector3(c.r, c.g, c.b))
		if std.normal_enabled and std.normal_texture != null:
			m.set_shader_parameter("normal_tex", HeroLook.mipmapped(std.normal_texture))
			m.set_shader_parameter("has_normal", true)
		var orm: Texture2D = std.roughness_texture if std.roughness_texture != null else std.metallic_texture
		if orm == null:
			orm = std.ao_texture
		if orm != null:
			m.set_shader_parameter("orm_tex", HeroLook.mipmapped(orm))
		if std.emission_enabled and std.emission_texture != null:
			m.set_shader_parameter("emissive_tex", HeroLook.mipmapped(std.emission_texture))
		else:
			m.set_shader_parameter("emissive_strength", 0.0)
	return m


## Shows one LOD at a time: SM_<slug> up close, SM_<slug>_LOD1..3 further out, at the distances where a hull
## `length_units` long shrinks to each LOD's screen size (Unreal's set_lods steps).
static func apply_lods(hull: Node, hero: Dictionary, length_units: float) -> int:
	var lods: Array = hero.get("lods", [])
	if lods.size() < 2:
		return 0
	var set_count := 0
	for i in lods.size():
		var node := _find(hull, str(lods[i].get("node", "")))
		if node == null:
			continue
		var begin := 0.0 if i == 0 else lod_distance(length_units, float(lods[i].get("screen", 1.0)))
		var end := 0.0 if i == lods.size() - 1 else lod_distance(length_units, float(lods[i + 1].get("screen", 0.0)))
		for gi: GeometryInstance3D in _geometry(node):
			gi.visibility_range_begin = begin
			gi.visibility_range_end = end
			gi.visibility_range_fade_mode = GeometryInstance3D.VISIBILITY_RANGE_FADE_DISABLED
		set_count += 1
	return set_count


## Camera distance where a hull `length_units` long covers `screen` of the view height (hero_gltf_manifest.py).
static func lod_distance(length_units: float, screen: float) -> float:
	if screen <= 0.0:
		return 0.0
	return length_units * 0.5 / (screen * tan(deg_to_rad(CAMERA_FOV) * 0.5))


static func _find(root: Node, node_name: String) -> Node:
	if node_name == "":
		return null
	if root.name == node_name:
		return root
	return root.find_child(node_name, true, false)


static func _geometry(node: Node) -> Array:
	var out: Array = []
	if node is GeometryInstance3D:
		out.append(node)
	for c in node.get_children():
		if c is GeometryInstance3D:
			out.append(c)
	return out


static func _texture(root_dir: String, rel: String) -> Texture2D:
	if rel == "":
		return null
	var path := rel if rel.is_absolute_path() else root_dir.path_join(rel)
	if _textures.has(path):
		return _textures[path]
	var tex: Texture2D = null
	if FileAccess.file_exists(path):
		var img := Image.load_from_file(path)
		if img != null and not img.is_empty():
			img.generate_mipmaps()
			tex = ImageTexture.create_from_image(img)
	_textures[path] = tex
	return tex
