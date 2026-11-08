extends SceneTree
## Turntable: renders asset keys to a contact sheet of 8 angles (ART-BIBLE.md section 11, the review method).
## Run through turntable.py (it sets the renderer and Dummy audio). Direct use:
##   Godot --path remaster/godot --audio-driver Dummy [--rendering-method gl_compatibility --rendering-driver opengl3] \
##         -s <abs path>/turntable.gd -- --keys=ship.fed.galaxy,planet.m --out=<dir> [--size=384] [--elev=14]
## Keys: ship.* (hero glb when LocalAssets resolves, else the procedural ShipFactory hull), planet.* (class sphere).
## The light rig mirrors godot/scenes/battle/battle_environment.gd (sun, fill, rim, ambient, ACES, glow settings).
## Output per key: <out>/<key>.sheet.png (4 x 2 grid, angle 0..315 degrees, a caption naming key, source and renderer)
## and <out>/<key>.json. The camera sits at a fixed elevation; the subject spins on its up axis.

const ANGLES := 8
const SUBJECT_SIZE := 2.0 ## Ship length (or planet diameter) in turntable units.
const FACTION_OWNER := {"fed": 1, "kli": 2, "rom": 3, "car": 4, "fer": 5}
## Class colours from godot/scenes/ui/screens/system_screen.gd PlanetOrb (the only planet look the game has today).
const PLANET_COLORS := {"m": "#4fae6a", "l": "#7fa35a", "o": "#3f7fd0", "h": "#d0a060", "k": "#9a8a80", "p": "#cfe3f0", "y": "#c0503a",
	"gasgiant": "#d9a86a", "asteroids": "#707078"}

var _opts := {"keys": "", "out": "user://turntable", "size": "384", "elev": "14", "bg": "#05070d", "rig": "show", "planets": "", "variant": "1"}
var _assets: BattleAssets


func _initialize() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--") and "=" in arg:
			var kv := arg.substr(2).split("=", true, 1)
			_opts[kv[0]] = kv[1]
	_run.call_deferred()


func _run() -> void:
	var keys: PackedStringArray = String(_opts.keys).split(",", false)
	if keys.is_empty():
		printerr("turntable: --keys=<key[,key...]> is required")
		quit(2)
		return
	var out_dir := ProjectSettings.globalize_path(String(_opts.out))
	DirAccess.make_dir_recursive_absolute(out_dir)
	_assets = BattleAssets.new()
	var failures := 0
	for key in keys:
		var ok: bool = await _render_key(key.strip_edges(), out_dir)
		if not ok:
			failures += 1
	quit(1 if failures > 0 else 0)


func _render_key(key: String, out_dir: String) -> bool:
	var info := {"key": key, "renderer": RenderingServer.get_current_rendering_method(),
		"driver": RenderingServer.get_current_rendering_driver_name(), "audio": AudioServer.get_driver_name() if AudioServer.has_method("get_driver_name") else "n/a"}
	var subject := _make_subject(key, info)
	if subject == null:
		printerr("turntable: cannot build ", key)
		return false
	var s := int(_opts.size)
	var vp := SubViewport.new()
	vp.size = Vector2i(s, s)
	vp.own_world_3d = true
	vp.transparent_bg = false
	vp.msaa_3d = Viewport.MSAA_4X
	vp.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(vp)
	_add_rig(vp)
	var pivot := Node3D.new()
	vp.add_child(pivot)
	pivot.add_child(subject)
	var cam := Camera3D.new()
	cam.fov = 32.0
	var elev := deg_to_rad(float(_opts.elev))
	var dist := SUBJECT_SIZE * 2.6
	cam.position = Vector3(0.0, sin(elev) * dist, cos(elev) * dist)
	vp.add_child(cam)
	cam.look_at(Vector3.ZERO, Vector3.UP)
	cam.current = true

	var frames: Array[Image] = []
	for i in ANGLES:
		pivot.rotation_degrees.y = i * (360.0 / ANGLES)
		for _f in 3:
			await process_frame
		await RenderingServer.frame_post_draw
		frames.append(vp.get_texture().get_image())
	var sheet := await _compose(frames, s, key, info)
	var png := out_dir.path_join(key + ".sheet.png")
	var err := sheet.save_png(png)
	if err != OK:
		printerr("turntable: cannot write ", png, " (", error_string(err), ")")
		return false
	var f := FileAccess.open(out_dir.path_join(key + ".json"), FileAccess.WRITE)
	f.store_string(JSON.stringify(info, "\t"))
	f.close()
	print("turntable: ", png, " ", sheet.get_width(), "x", sheet.get_height(), " ", info.renderer, " ", info.get("source", ""))
	vp.queue_free()
	return true


func _add_rig(parent: Node) -> void:
	var env := Environment.new()
	env.background_mode = Environment.BG_COLOR
	env.background_color = Color(String(_opts.bg))
	env.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	env.ambient_light_color = Color(0.26, 0.27, 0.33) # battle_environment.gd
	env.ambient_light_energy = 1.0
	env.tonemap_mode = Environment.TONE_MAPPER_ACES
	env.tonemap_exposure = 1.0
	env.tonemap_white = 8.0
	env.glow_enabled = true
	env.glow_intensity = 0.4
	env.glow_hdr_threshold = 1.8
	env.glow_hdr_scale = 1.2
	var we := WorldEnvironment.new()
	we.environment = env
	parent.add_child(we)
	var sun := DirectionalLight3D.new()
	sun.light_color = Color(1.0, 0.93, 0.82)
	sun.light_energy = 3.0
	sun.rotation_degrees = Vector3(-24.0, -128.0, 0.0)
	if String(_opts.rig) == "show":
		# Show look (ART-BIBLE combat benchmark): one hard key, deep shadows, almost no ambient, a faint cool edge light.
		env.ambient_light_energy = 0.04
		env.tonemap_mode = Environment.TONE_MAPPER_FILMIC
		env.tonemap_white = 6.0
		sun.light_energy = 4.5
		sun.shadow_enabled = true
		sun.shadow_normal_bias = 2.5
		sun.shadow_bias = 0.05
		parent.add_child(sun)
		var edge := DirectionalLight3D.new()
		edge.light_color = Color(0.6, 0.7, 1.0)
		edge.light_energy = 0.12
		edge.rotation_degrees = Vector3(30.0, 50.0, 0.0)
		parent.add_child(edge)
		return
	parent.add_child(sun)
	var fill := DirectionalLight3D.new()
	fill.light_color = Color(0.75, 0.8, 1.0)
	fill.light_energy = 0.9
	fill.rotation_degrees = Vector3(-35.0, 52.0, 0.0)
	parent.add_child(fill)
	var rim := DirectionalLight3D.new()
	rim.light_color = Color(0.6, 0.66, 0.9)
	rim.light_energy = 0.7
	rim.rotation_degrees = Vector3(35.0, 40.0, 0.0)
	parent.add_child(rim)


func _make_subject(key: String, info: Dictionary) -> Node3D:
	var parts := key.split(".")
	if parts.size() < 2:
		return null
	if parts[0] == "ship":
		return _ship(key, parts[1], info)
	if parts[0] == "planet":
		return _planet(parts[1], info)
	printerr("turntable: only ship.* and planet.* keys render (got ", key, ")")
	return null


func _ship(key: String, fac: String, info: Dictionary) -> Node3D:
	var holder := Node3D.new()
	var hull: Node3D = _assets.instantiate_hero(key) if _assets != null else null
	if hull != null:
		var hero: Dictionary = hull.get_meta("botf_hero")
		var canon := float(hero.get("lengthMeters", 0.0))
		var box := _aabb(hull)
		if canon <= 0.0:
			canon = maxf(box.size.x, 1e-3)
		var sc := SUBJECT_SIZE / canon
		hull.transform = Transform3D(Basis.from_scale(Vector3.ONE * sc), -box.get_center() * sc)
		HeroLook.apply(hull, hero, str(hull.get_meta("botf_hero_root", "")))
		info["source"] = "hero glb (third-party, private)"
		info["length_m"] = canon
	else:
		var station := (parts_last(key) in ["starbase", "outpost", "platform", "spacedock", "nor"])
		hull = ShipFactory.build(int(FACTION_OWNER.get(fac, 1)), station, {"assetKey": key, "name": parts_last(key)})
		hull.scale = Vector3.ONE * SUBJECT_SIZE
		var box2 := _aabb(hull)
		hull.position = -box2.get_center()
		info["source"] = "procedural (ShipFactory)"
		info["length_m"] = ShipFactory.CANON_LENGTHS.get(key, 0)
	holder.add_child(hull)
	return holder


func parts_last(key: String) -> String:
	return key.split(".")[-1]


func _planet(cls: String, info: Dictionary) -> Node3D:
	var holder := Node3D.new()
	var col := Color(String(PLANET_COLORS.get(cls, "#8899aa")))
	info["source"] = "placeholder class-colour sphere (no planet art exists yet)"
	if String(_opts.planets) != "":
		if cls == "asteroids":
			if _textured_asteroids(holder, info):
				return holder
		elif _textured_planet(holder, cls, info):
			return holder
	if cls == "gasgiant":
		var mi := MeshInstance3D.new()
		var sph := SphereMesh.new()
		sph.radius = SUBJECT_SIZE * 0.5
		sph.height = SUBJECT_SIZE
		sph.radial_segments = 96
		sph.rings = 48
		mi.mesh = sph
		var pm := ShaderMaterial.new()
		pm.shader = load("res://scenes/battle/shaders/planet.gdshader")
		mi.material_override = pm
		holder.add_child(mi)
		info["source"] = "procedural (planet.gdshader banded sphere)"
	elif cls == "asteroids":
		var rng := RandomNumberGenerator.new()
		rng.seed = 8472
		for _i in 40:
			var mi2 := MeshInstance3D.new()
			var sm := SphereMesh.new()
			var r := rng.randf_range(0.04, 0.16)
			sm.radius = r
			sm.height = r * 2.0 * rng.randf_range(0.7, 1.0)
			sm.radial_segments = 12
			sm.rings = 6
			mi2.mesh = sm
			mi2.position = Vector3(rng.randf_range(-1, 1), rng.randf_range(-0.25, 0.25), rng.randf_range(-1, 1))
			var m2 := StandardMaterial3D.new()
			m2.albedo_color = col.darkened(rng.randf_range(0.0, 0.4))
			m2.roughness = 0.95
			mi2.material_override = m2
			holder.add_child(mi2)
	else:
		var mi3 := MeshInstance3D.new()
		var sph3 := SphereMesh.new()
		sph3.radius = SUBJECT_SIZE * 0.5
		sph3.height = SUBJECT_SIZE
		sph3.radial_segments = 96
		sph3.rings = 48
		mi3.mesh = sph3
		var m3 := StandardMaterial3D.new()
		m3.albedo_color = col
		m3.roughness = 0.85
		mi3.material_override = m3
		holder.add_child(mi3)
	return holder


## L7: planet texture sets from LocalAssets/planets/<class>/{desktop,web}/ (tools/art/gen_planets.py). Desktop renderers use
## the 4096x2048 albedo, the normal map and a cloud shell; Compatibility uses the 1024x512 web albedo (relief baked in).
func _textured_planet(holder: Node3D, cls: String, info: Dictionary) -> bool:
	var web := RenderingServer.get_current_rendering_method() == "gl_compatibility"
	var v := int(_opts.variant)
	var dir := String(_opts.planets).path_join(cls).path_join("web" if web else "desktop")
	var alb_path := dir.path_join("v%d_albedo.%s" % [v, "webp" if web else "png"])
	if not FileAccess.file_exists(alb_path):
		return false
	var mi := MeshInstance3D.new()
	var sph := SphereMesh.new()
	sph.radius = SUBJECT_SIZE * 0.5
	sph.height = SUBJECT_SIZE
	sph.radial_segments = 128 if not web else 48
	sph.rings = 64 if not web else 24
	mi.mesh = sph
	var m := StandardMaterial3D.new()
	m.albedo_texture = ImageTexture.create_from_image(Image.load_from_file(alb_path))
	m.roughness = 0.9 if cls != "gasgiant" else 0.6
	var npath := dir.path_join("v%d_normal.png" % v)
	if not web and FileAccess.file_exists(npath):
		m.normal_enabled = true
		m.normal_texture = ImageTexture.create_from_image(Image.load_from_file(npath))
		m.normal_scale = 1.0
	mi.material_override = m
	holder.add_child(mi)
	var cpath := dir.path_join("v%d_clouds.png" % v)
	if FileAccess.file_exists(cpath):
		var cimg := Image.load_from_file(cpath)
		cimg.convert(Image.FORMAT_L8)
		var rgba := Image.create(cimg.get_width(), cimg.get_height(), false, Image.FORMAT_RGBA8)
		var raw := cimg.get_data()
		var out := PackedByteArray()
		out.resize(raw.size() * 4)
		for i in raw.size():
			out[i * 4] = 255
			out[i * 4 + 1] = 255
			out[i * 4 + 2] = 255
			out[i * 4 + 3] = int(raw[i] * 0.92)
		rgba = Image.create_from_data(cimg.get_width(), cimg.get_height(), false, Image.FORMAT_RGBA8, out)
		var cm := MeshInstance3D.new()
		var cs := SphereMesh.new()
		cs.radius = SUBJECT_SIZE * 0.5 * 1.012
		cs.height = SUBJECT_SIZE * 1.012
		cs.radial_segments = sph.radial_segments
		cs.rings = sph.rings
		cm.mesh = cs
		var mm := StandardMaterial3D.new()
		mm.albedo_texture = ImageTexture.create_from_image(rgba)
		mm.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
		mm.roughness = 1.0
		cm.material_override = mm
		holder.add_child(cm)
	info["source"] = "procedural texture set v%d (%s)" % [v, "web 1024x512" if web else "desktop 4096x2048 + normal"]
	return true


## L7: the asteroid belt from the 6 generated rock meshes (rock_<n>.obj, spherical UVs) and the rock albedo/normal.
func _textured_asteroids(holder: Node3D, info: Dictionary) -> bool:
	var web := RenderingServer.get_current_rendering_method() == "gl_compatibility"
	var dir := String(_opts.planets).path_join("asteroids").path_join("web" if web else "desktop")
	var meshes: Array[ArrayMesh] = []
	for i in range(1, 7):
		var p := dir.path_join("rock_%d.obj" % i)
		if not FileAccess.file_exists(p):
			return false
		meshes.append(_load_obj(p))
	var m := StandardMaterial3D.new()
	var a := dir.path_join("rock_albedo.webp" if web else "rock_albedo.png")
	if FileAccess.file_exists(a):
		m.albedo_texture = ImageTexture.create_from_image(Image.load_from_file(a))
	var n := dir.path_join("rock_normal.png")
	if not web and FileAccess.file_exists(n):
		m.normal_enabled = true
		m.normal_texture = ImageTexture.create_from_image(Image.load_from_file(n))
	m.roughness = 0.95
	var rng := RandomNumberGenerator.new()
	rng.seed = 8472
	for _i in 60:
		var mi := MeshInstance3D.new()
		mi.mesh = meshes[rng.randi() % 6]
		mi.scale = Vector3.ONE * rng.randf_range(0.06, 0.2)
		mi.rotation = Vector3(rng.randf() * TAU, rng.randf() * TAU, rng.randf() * TAU)
		mi.position = Vector3(rng.randf_range(-1, 1), rng.randf_range(-0.25, 0.25), rng.randf_range(-1, 1))
		mi.material_override = m
		holder.add_child(mi)
	info["source"] = "procedural rock meshes x6 + rock maps (%s)" % ("web" if web else "desktop")
	return true


func _load_obj(path: String) -> ArrayMesh:
	var verts := PackedVector3Array()
	var uvs := PackedVector2Array()
	var idx := PackedInt32Array()
	for line in FileAccess.get_file_as_string(path).split("
", false):
		var t := line.split(" ", false)
		if t.size() < 3:
			continue
		if t[0] == "v":
			verts.append(Vector3(float(t[1]), float(t[2]), float(t[3])))
		elif t[0] == "vt":
			uvs.append(Vector2(float(t[1]), float(t[2])))
		elif t[0] == "f":
			for k in 3:
				idx.append(int(t[k + 1].split("/")[0]) - 1)
	var st := SurfaceTool.new()
	st.begin(Mesh.PRIMITIVE_TRIANGLES)
	for i in idx:
		st.set_uv(uvs[i])
		st.add_vertex(verts[i])
	st.generate_normals()
	return st.commit()


func _aabb(n: Node3D) -> AABB:
	var out := AABB()
	var first := true
	for mi in n.find_children("*", "MeshInstance3D", true, false):
		var m := mi as MeshInstance3D
		if m.mesh == null:
			continue
		var xf := Transform3D.IDENTITY
		var cur: Node = m
		while cur != null and cur != n:
			if cur is Node3D:
				xf = (cur as Node3D).transform * xf
			cur = cur.get_parent()
		var b := xf * m.mesh.get_aabb()
		out = b if first else out.merge(b)
		first = false
	return out


func _compose(frames: Array[Image], s: int, key: String, info: Dictionary) -> Image:
	var head := 34
	var cols := 4
	var vp := SubViewport.new()
	vp.size = Vector2i(cols * s, 2 * s + head)
	vp.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(vp)
	var bg := ColorRect.new()
	bg.color = Color(String(_opts.bg))
	bg.size = Vector2(vp.size)
	vp.add_child(bg)
	for i in frames.size():
		var tr := TextureRect.new()
		tr.texture = ImageTexture.create_from_image(frames[i])
		tr.position = Vector2((i % cols) * s, head + (i / cols) * s)
		tr.size = Vector2(s, s)
		vp.add_child(tr)
		var lb := Label.new()
		lb.text = "%d deg" % int(i * (360.0 / ANGLES))
		lb.position = tr.position + Vector2(6, 4)
		lb.add_theme_font_size_override("font_size", 13)
		lb.add_theme_color_override("font_color", Color(0.7, 0.8, 1.0, 0.85))
		vp.add_child(lb)
	var cap := Label.new()
	cap.text = "%s | %s | %s %s | elev %s deg | rig %s" % [key, info.get("source", ""), info.renderer, info.driver, _opts.elev, _opts.rig]
	cap.position = Vector2(8, 8)
	cap.add_theme_font_size_override("font_size", 15)
	vp.add_child(cap)
	for _f in 3:
		await process_frame
	await RenderingServer.frame_post_draw
	var img := vp.get_texture().get_image()
	vp.queue_free()
	return img
