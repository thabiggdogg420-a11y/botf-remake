extends SceneTree
## Bakes the menu ship icons from the hero models (LocalAssets, third-party-derived: never committed).
##   godot --path remaster/godot --resolution 640x640 --audio-driver Dummy -s res://tools/bake_ship_icons.gd -- [out_dir] [filter] [--skip-existing]
## For every key in godot-hero-manifest.json the hull is loaded through BattleShip / BattleAssets (the game's own hero loader and
## HeroLook materials), lit on a transparent background and rendered twice: "iso" (three-quarter top view) and "top" (clean
## top-down, bow to the right). Output: <out_dir>/<fac>_<class>_<view>.png (256 px) and ..._<view>_64.png (64 px).
## out_dir defaults to $BOTF_SHIP_ICONS, else ship_icons beside the resolved manifest (LocalAssets/ship_icons).
## filter: only keys containing this text (e.g. "fed.galaxy"). ShipIcons finds the files again at run time.

const SIZE := 512
const FOV := 28.0

var out_dir := ""
var filter := ""
var skip_existing := false
var vp: SubViewport
var cam: Camera3D
var assets: BattleAssets

func _initialize() -> void:
	var a := OS.get_cmdline_user_args()
	for s in a:
		if s == "--skip-existing":
			skip_existing = true
		elif out_dir == "":
			out_dir = s
		else:
			filter = s
	assets = BattleAssets.new()
	if out_dir == "":
		out_dir = OS.get_environment("BOTF_SHIP_ICONS")
	if out_dir == "" and assets.hero_manifest_path != "":
		out_dir = assets.hero_manifest_path.get_base_dir().get_base_dir().path_join("ship_icons")
	if out_dir == "" or assets.heroes.is_empty():
		push_error("bake_ship_icons: no hero manifest found (set BOTF_HERO_MANIFEST) or no output folder")
		quit(1)
		return
	DirAccess.make_dir_recursive_absolute(out_dir)
	_stage()
	_run.call_deferred()

func _stage() -> void:
	vp = SubViewport.new()
	vp.size = Vector2i(SIZE, SIZE)
	vp.transparent_bg = true
	vp.own_world_3d = true
	vp.msaa_3d = Viewport.MSAA_4X
	vp.render_target_update_mode = SubViewport.UPDATE_DISABLED
	root.add_child(vp)
	var env := Environment.new()
	env.background_mode = Environment.BG_CLEAR_COLOR
	env.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	env.ambient_light_color = Color(0.46, 0.5, 0.6)
	env.ambient_light_energy = 0.9
	env.tonemap_mode = Environment.TONE_MAPPER_FILMIC
	var we := WorldEnvironment.new()
	we.environment = env
	vp.add_child(we)
	var key := DirectionalLight3D.new()
	key.rotation_degrees = Vector3(-52.0, 38.0, 0.0)
	key.light_energy = 1.9
	vp.add_child(key)
	var fill := DirectionalLight3D.new()
	fill.rotation_degrees = Vector3(-28.0, -145.0, 0.0)
	fill.light_energy = 0.7
	fill.light_color = Color(0.7, 0.8, 1.0)
	vp.add_child(fill)
	cam = Camera3D.new()
	cam.fov = FOV
	cam.near = 0.05
	cam.far = 4000.0
	vp.add_child(cam)
	cam.current = true

func _frames(n: int = 2) -> void:
	for i in n:
		await process_frame

func _run() -> void:
	var keys: Array = assets.heroes.keys()
	keys.sort()
	var done := 0
	var failed: Array = []
	for key in keys:
		if filter != "" and str(key).find(filter) < 0:
			continue
		var stem := str(key).trim_prefix("ship.").replace(".", "_")
		if skip_existing and FileAccess.file_exists(out_dir.path_join(stem + "_top.png")):
			continue
		var ok := await _bake(str(key), stem)
		if ok:
			done += 1
		else:
			failed.append(key)
	print("baked ", done, " classes to ", out_dir, "  failed: ", failed)
	quit(0 if failed.is_empty() else 2)

func _bake(key: String, stem: String) -> bool:
	var ship := BattleShip.new()
	vp.add_child(ship)
	var klass := key.get_slice(".", key.get_slice_count(".") - 1)
	ship.setup({"id": 1, "owner": 0, "name": klass, "assetKey": key, "displayLength": 40.0}, assets)
	if not ship.imported:
		ship.queue_free()
		return false
	ship.shield.visible = false
	_lod0_only(ship, key)
	await _frames(2)
	var box := _aabb(ship)
	if box.size.length() < 0.001:
		ship.queue_free()
		return false
	var c := box.get_center()
	var half := maxf(box.size.x, box.size.z) * 0.5
	for view in ["iso", "top"]:
		var fov_t := tan(deg_to_rad(FOV * 0.5))
		if view == "top":
			var d := (half * 1.22) / fov_t + box.size.y * 0.5
			cam.position = c + Vector3(0, d, 0)
			cam.look_at(c, Vector3(0, 0, -1))
		else:
			var r := box.size.length() * 0.5 * 0.98
			var dir := Vector3(-0.55, 0.72, 0.85).normalized()
			cam.position = c + dir * (r / fov_t)
			cam.look_at(c, Vector3.UP)
		vp.render_target_update_mode = SubViewport.UPDATE_ONCE
		await _frames(2)
		await RenderingServer.frame_post_draw
		var img := vp.get_texture().get_image()
		img.convert(Image.FORMAT_RGBA8)
		img.fix_alpha_edges()
		var big := img.duplicate() as Image
		big.resize(256, 256, Image.INTERPOLATE_LANCZOS)
		var small := img.duplicate() as Image
		small.resize(64, 64, Image.INTERPOLATE_LANCZOS)
		big.save_png(out_dir.path_join("%s_%s.png" % [stem, view]))
		small.save_png(out_dir.path_join("%s_%s_64.png" % [stem, view]))
	ship.queue_free()
	await _frames(1)
	return true

## Shows only the closest LOD of the hull and switches off every distance fade, so the icon is the full-detail mesh.
func _lod0_only(ship: BattleShip, key: String) -> void:
	var hero: Dictionary = assets.heroes.get(key, {})
	var lods: Array = hero.get("lods", [])
	for i in range(1, lods.size()):
		var n := ship.find_child(str(lods[i].get("node", "")), true, false)
		if n is Node3D:
			(n as Node3D).visible = false
	for g in ship.find_children("*", "GeometryInstance3D", true, false):
		(g as GeometryInstance3D).visibility_range_begin = 0.0
		(g as GeometryInstance3D).visibility_range_end = 0.0

func _aabb(ship: Node3D) -> AABB:
	var out := AABB()
	var first := true
	for g in ship.find_children("*", "GeometryInstance3D", true, false):
		var gi := g as GeometryInstance3D
		if gi == ship.shield or not gi.is_visible_in_tree():
			continue
		var b := gi.global_transform * gi.get_aabb()
		if first:
			out = b
			first = false
		else:
			out = out.merge(b)
	return out
