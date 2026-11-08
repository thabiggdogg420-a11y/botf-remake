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

var _opts := {"keys": "", "out": "user://turntable", "size": "384", "elev": "14", "bg": "#05070d"}
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
	cap.text = "%s | %s | %s %s | elev %s deg" % [key, info.get("source", ""), info.renderer, info.driver, _opts.elev]
	cap.position = Vector2(8, 8)
	cap.add_theme_font_size_override("font_size", 15)
	vp.add_child(cap)
	for _f in 3:
		await process_frame
	await RenderingServer.frame_post_draw
	var img := vp.get_texture().get_image()
	vp.queue_free()
	return img
