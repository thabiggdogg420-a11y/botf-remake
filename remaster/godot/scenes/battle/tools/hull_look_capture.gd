extends SceneTree
## Hull look proof (hull-look lane, 25 Nov 2026; Simon: "the main menu ships are always really low res and low poly and look
## terrible, they have all these little white specs all over them, and the starbases in the battles do too").
##
## Draws the title fleet of every empire (TitleFleet in a SubViewport set up like the title screen's) and the battle
## stations (BattleShip in the battle environment) at 1920x1080, saves the full frame plus a close-up per ship, and prints
## one line per ship:
##   HULLLOOK <kind> <label> hero=<key|none> tris=<n> maxtex=<px> mintex=<px> specks=<n> speckppm=<n per million lit px>
## With --assert it checks every model and close-up against the bars below and exits 1 on a miss (a "low-detail model" or a
## "speck" line says which). The bars are the contract that tools/verify/verify.ps1's hull look step runs.
##
##   <godot or exe> --path remaster/godot --resolution 1920x1080 --audio-driver Dummy -s res://scenes/battle/tools/hull_look_capture.gd -- \
##       <out_dir> [title|station|all] [--empires=1,2,3,4,5] [--assert] [--sliced] [--aa=msaa2|fxaa|off]
##
## Specks: a white-clipped pixel (every channel over SPECK_WHITE) that is at least SPECK_JUMP brighter than nearly every pixel of the ring
## SPECK_R pixels around it, i.e. a point of light a pixel or two across with dark hull round it. A lit window row reads as
## a line (its ring is half lit) and a hull rim as an edge; a window grid sampled below a pixel reads as scatter. Counted
## over the hull's own pixels of the close-up only (the sky and the star field are masked out by the sky luma).

const W := 1920
const H := 1080
## Bars. Title: the lead ship is the hero shot, escorts are smaller on screen. Stations: seen from close range in battle.
const MIN_TRIS_LEAD := 7000
const MIN_TRIS_ESCORT := 3000
const MIN_TRIS_STATION := 1000   # the Romulan starbase (1.2k) is the lowest on disk; nothing better is available offline
const MIN_TEX_LEAD := 2048
const MIN_TEX_ESCORT := 1024
const MIN_TEX_STATION := 1024
## Specks per million hull pixels allowed in a close-up.
const MAX_SPECK_PPM := 300.0   # after this lane the worst hull is 243; before it was 1431 (Galaxy), 1391, 1096
## Navigation lights must sit on the hull: the worst light's distance to the nearest hull vertex, as a share of hull length.
const MAX_NAV_OFF := 0.02
const SPECK_LUMA := 0.72
const SPECK_JUMP := 0.38
## A speck is a pixel the frame clipped to white: every channel at or over this (0-255). A window that reads as a window is
## warm or blue light under it; the hull-look fix keeps those and removes the clipped, sparkling white points.
const SPECK_WHITE := 235
const SPECK_R := 3

var _out := ""
var _assert := false
var _sliced := false
var _aa := "msaa2"
var _lines := PackedStringArray()
var _fails := PackedStringArray()
var _json := []


func _initialize() -> void:
	_run.call_deferred()


func _wait(n: int) -> void:
	for i in n:
		await process_frame


func _run() -> void:
	var args := OS.get_cmdline_user_args()
	_out = str(args[0]) if args.size() > 0 else "user://hull-look"
	var mode := "all"
	var empires := [1, 2, 3, 4, 5]
	for i in range(1, args.size()):
		var a := str(args[i])
		if a == "--assert":
			_assert = true
		elif a == "--sliced":
			_sliced = true
		elif a.begins_with("--aa="):
			_aa = a.trim_prefix("--aa=")
		elif a.begins_with("--empires="):
			empires = []
			for p in a.trim_prefix("--empires=").split(","):
				empires.append(int(p))
		elif a in ["title", "station", "all"]:
			mode = a
	DirAccess.make_dir_recursive_absolute(_out)
	AppLoader.mount_user_packs()
	await _wait(2)
	if mode in ["title", "all"]:
		for e in empires:
			await _title(int(e))
	if mode in ["station", "all"]:
		for e in empires:
			await _station(int(e), "Starbase")
			await _station(int(e), "Outpost")
	var f := FileAccess.open(_out.path_join("hull-look.json"), FileAccess.WRITE)
	f.store_string(JSON.stringify(_json, "  "))
	f.close()
	for l in _lines:
		print(l)
	for m in _fails:
		print("HULLLOOK FAIL ", m)
	print("HULLLOOK summary: %d ships, %d misses (%s)" % [_json.size(), _fails.size(), "asserted" if _assert else "report only"])
	quit(1 if (_assert and not _fails.is_empty()) else 0)


func _settings() -> Dictionary:
	return {"display.antiAlias": _aa}


func _viewport() -> SubViewport:
	var sv := SubViewport.new()
	sv.size = Vector2i(W, H)
	sv.own_world_3d = true
	sv.msaa_3d = Viewport.MSAA_2X
	sv.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	DisplaySettings.apply_viewport(sv, _settings())
	return sv


# --------------------------------------------------------------------------------------------------------------------
func _title(empire: int) -> void:
	var holder := Node.new()
	get_root().add_child(holder)
	var sv := _viewport()
	holder.add_child(sv)
	# The title attaches the web bloom to its container; in Forward+ it is a no-op, so the capture does the same call.
	var svc := SubViewportContainer.new()
	svc.stretch = false
	holder.add_child(svc)
	var fleet := TitleFleet.new()
	fleet.animate = false
	sv.add_child(fleet)
	var assets := BattleAssets.new()
	if _sliced:
		fleet.setup_sliced(empire, assets)
	else:
		fleet.setup(empire, assets)
	await _wait(4)
	var t0 := Time.get_ticks_msec()
	while (BattleAssets.loading() or fleet.ships.size() < (TitleFleet.HEROES[empire] as Array).size()) and Time.get_ticks_msec() - t0 < 180000:
		await _wait(1)
	await _wait(120)
	var full := sv.get_texture().get_image()
	full.save_png(_out.path_join("title-e%d.png" % empire))
	for i in fleet.ships.size():
		var ship: BattleShip = fleet.ships[i]
		var lead := i == 0
		var st := _stats(ship)
		# Close-up: the fleet's own camera direction, pulled in until the hull's bounding sphere fills the frame.
		var box := _bounds(ship)
		var centre := box.get_center()
		var radius := box.size.length() * 0.5
		var dir := (fleet.camera.global_position - centre).normalized()
		var dist := radius / sin(deg_to_rad(fleet.camera.fov * 0.5)) * 0.78
		var keep := fleet.camera.global_transform
		fleet.camera.global_position = centre + dir * dist
		fleet.camera.look_at(centre, Vector3.UP)
		await _wait(60)
		var img := sv.get_texture().get_image()
		img.save_png(_out.path_join("title-e%d-s%d.png" % [empire, i]))
		var bg := await _background(fleet, sv)
		bg.save_png(_out.path_join("title-e%d-s%d-bg.png" % [empire, i]))
		var nav := _nav_screen(ship, fleet.camera)   # with the close-up camera, before it goes back
		fleet.camera.global_transform = keep
		var sp := _specks(img, bg, nav)
		st["nav_off"] = _nav_off(ship)
		_record("title", "e%d-s%d %s" % [empire, i, ship.ship_name], st, sp, MIN_TRIS_LEAD if lead else MIN_TRIS_ESCORT, MIN_TEX_LEAD if lead else MIN_TEX_ESCORT)
	holder.queue_free()
	await _wait(8)


func _station(empire: int, klass: String) -> void:
	var root := Node3D.new()
	get_root().add_child(root)
	var sv := _viewport()
	root.add_child(sv)
	var world := Node3D.new()
	sv.add_child(world)
	var env := BattleEnvironment.build(world, Vector2(16, 12) * BattleScene.CELL)
	var sun: DirectionalLight3D = env.sun
	sun.rotation_degrees = Vector3(-28.0, -38.0, 0.0)
	sun.light_energy = CompatLook.sun(2.4)
	var assets := BattleAssets.shared()
	var ship := BattleShip.new()
	world.add_child(ship)
	# A real battle's station dict (WebBridge tactical): "<Type> <n>" as its name, stationType, no assetKey.
	ship.setup({"id": 2000000 + empire, "owner": empire, "name": "%s 1" % klass, "stationType": klass, "maxHull": 300, "referenceShipId": -1, "station": true}, assets)
	var cam := Camera3D.new()
	cam.fov = 48.0   # the battle camera's
	cam.near = 0.5
	cam.far = 6000.0
	world.add_child(cam)
	cam.current = true
	var t0 := Time.get_ticks_msec()
	while BattleAssets.loading() and Time.get_ticks_msec() - t0 < 180000:
		await _wait(1)
	await _wait(60)
	var box := _bounds(ship)
	var centre := box.get_center()
	var radius := box.size.length() * 0.5
	var dir := Vector3(0.45, 0.28, 0.85).normalized()
	var st := _stats(ship)
	# Two framings: the battle's typical (hull fills about a third of the frame) and the close-up (fills it).
	var shots := [["battle", 1.9], ["close", 0.86]]
	for s in shots:
		cam.position = centre + dir * (radius / sin(deg_to_rad(cam.fov * 0.5)) * float(s[1]))
		cam.look_at(centre, Vector3.UP)
		await _wait(60)
		var img := sv.get_texture().get_image()
		img.save_png(_out.path_join("station-e%d-%s-%s.png" % [empire, klass.to_lower(), s[0]]))
		if s[0] == "close":
			var bg := await _background(ship, sv)
			bg.save_png(_out.path_join("station-e%d-%s-bg.png" % [empire, klass.to_lower()]))
			var sp := _specks(img, bg, _nav_screen(ship, cam))
			st["nav_off"] = _nav_off(ship)
			_record("station", "e%d-%s" % [empire, klass.to_lower()], st, sp, MIN_TRIS_STATION, MIN_TEX_STATION)
	root.queue_free()
	await _wait(8)


# --------------------------------------------------------------------------------------------------------------------
func _record(kind: String, label: String, st: Dictionary, sp: Dictionary, min_tris: int, min_tex: int) -> void:
	var hero := str(st.hero)
	_lines.append("HULLLOOK %s %s hero=%s tris=%d maxtex=%d mintex=%d specks=%d speckppm=%.1f navoff=%.3f hullpx=%d" % [
		kind, label, hero if hero != "" else "none", st.tris, st.maxtex, st.mintex, sp.count, sp.ppm, float(st.get("nav_off", 0.0)), sp.lit])
	var row := {"kind": kind, "label": label, "hero": hero, "tris": st.tris, "maxtex": st.maxtex, "mintex": st.mintex, "specks": sp.count, "speck_ppm": sp.ppm, "nav_off": st.get("nav_off", 0.0)}
	_json.append(row)
	if hero == "":
		_fails.append("%s %s: procedural stand-in hull (no hero model loaded)" % [kind, label])
	if int(st.tris) < min_tris:
		_fails.append("%s %s: low-detail model, %d triangles (bar %d)" % [kind, label, st.tris, min_tris])
	if int(st.maxtex) < min_tex and not bool(st.plated):   # a plated hull (untextured model, baked vertex colour) has no texture to measure
		_fails.append("%s %s: low-resolution textures, largest %d px (bar %d)" % [kind, label, st.maxtex, min_tex])
	if float(st.get("nav_off", 0.0)) > MAX_NAV_OFF:
		_fails.append("%s %s: a navigation light floats %.1f%% of the hull length off the hull (bar %.0f%%), a white or red speck beside the ship" % [kind, label, float(st.nav_off) * 100.0, MAX_NAV_OFF * 100.0])
	if float(sp.ppm) > MAX_SPECK_PPM:
		_fails.append("%s %s: white specks, %.0f per million hull pixels (bar %.0f)" % [kind, label, sp.ppm, MAX_SPECK_PPM])


## Triangles of the visible hull meshes and the textures its materials carry (shader parameters and standard materials).
func _stats(ship: BattleShip) -> Dictionary:
	var tris := 0
	var maxtex := 0
	var mintex := 1 << 30
	var seen := {}
	var plated := false
	for n in ship.find_children("*", "MeshInstance3D", true, false):
		var mi := n as MeshInstance3D
		if mi.mesh == null or not mi.is_visible_in_tree() or mi.name.begins_with("NavLight") or "hield" in str(mi.name):
			continue
		for s in mi.mesh.get_surface_count():
			var arr := mi.mesh.surface_get_arrays(s)
			var idx = arr[Mesh.ARRAY_INDEX]
			if idx != null and (idx as PackedInt32Array).size() > 0:
				tris += (idx as PackedInt32Array).size() / 3
			elif arr[Mesh.ARRAY_VERTEX] != null:
				tris += (arr[Mesh.ARRAY_VERTEX] as PackedVector3Array).size() / 3
			var mat: Material = mi.get_surface_override_material(s)
			if mat == null:
				mat = mi.material_override
			if mat == null:
				mat = mi.mesh.surface_get_material(s)
			if mat == null or seen.has(mat):
				continue
			seen[mat] = true
			if mat is ShaderMaterial and (mat as ShaderMaterial).shader == HeroLook.PLATED_SHADER:
				plated = true
			for tex in _textures_of(mat):
				var px := maxi(tex.get_width(), tex.get_height())
				maxtex = maxi(maxtex, px)
				mintex = mini(mintex, px)
	if mintex == 1 << 30:
		mintex = 0
	return {"hero": ship.hero_key, "tris": tris, "maxtex": maxtex, "mintex": mintex, "plated": plated}


func _textures_of(mat: Material) -> Array:
	var out := []
	if mat is ShaderMaterial:
		var sm := mat as ShaderMaterial
		for p in ["bc_tex", "normal_tex", "orm_tex", "emissive_tex"]:
			var t = sm.get_shader_parameter(p)
			if t is Texture2D:
				out.append(t)
	elif mat is BaseMaterial3D:
		var bm := mat as BaseMaterial3D
		if bm.albedo_texture != null:
			out.append(bm.albedo_texture)
	return out


func _bounds(ship: Node3D) -> AABB:
	var out := AABB()
	var first := true
	for n in ship.find_children("*", "MeshInstance3D", true, false):
		var mi := n as MeshInstance3D
		if mi.mesh == null or not mi.is_visible_in_tree() or mi.name.begins_with("NavLight") or "hield" in str(mi.name):
			continue
		var b := mi.global_transform * mi.get_aabb()
		out = b if first else out.merge(b)
		first = false
	return out


## The same frame with the ships hidden (the sky, stars and dust alone), so the hull's own pixels are the ones that differ.
## `hide` is the node whose ships go (the title fleet's ships, or one station).
func _background(hide: Node, sv: SubViewport) -> Image:
	var gone: Array = []
	if hide is TitleFleet:
		gone = (hide as TitleFleet).ships.duplicate()
	else:
		gone = [hide]
	for s in gone:
		(s as Node3D).visible = false
	await _wait(2)
	var bg := sv.get_texture().get_image()
	for s in gone:
		(s as Node3D).visible = true
	await _wait(2)
	return bg


func _near(pts: Array, x: int, y: int) -> bool:
	for p: Vector2 in pts:
		if absf(p.x - x) < 14.0 and absf(p.y - y) < 14.0:
			return true
	return false


## Screen positions of a ship's navigation lights.
func _nav_screen(ship: Node, cam: Camera3D) -> Array:
	var out := []
	for n in ship.find_children("NavLight*", "MeshInstance3D", true, false):
		if not cam.is_position_behind((n as Node3D).global_position):
			out.append(cam.unproject_position((n as Node3D).global_position))
	return out


## How far the navigation lights sit off the hull, as a share of the hull's length: the worst light's distance to the nearest
## drawn vertex. Lights that float in open space (the bounding-box corners, before this lane) read as white specks beside the hull.
func _nav_off(ship: BattleShip) -> float:
	var pts := PackedVector3Array()
	var stack := [[ship, Transform3D.IDENTITY]]
	var lights := []
	while not stack.is_empty():
		var item = stack.pop_back()
		var node: Node = item[0]
		var t: Transform3D = item[1]
		if node is MeshInstance3D and (node as MeshInstance3D).mesh != null and str(node.name).begins_with("NavLight"):
			lights.append(t.origin)
		elif node is MeshInstance3D and (node as MeshInstance3D).mesh != null and (node as MeshInstance3D).visible and not str(node.name).begins_with("Flagship") and not "hield" in str(node.name):
			var f: PackedVector3Array = ((node as MeshInstance3D).mesh as Mesh).get_faces()
			var step := maxi(1, f.size() / 6000)
			for i in range(0, f.size(), step):
				pts.append(t * f[i])
		for c in node.get_children():
			if c is Node3D:
				stack.append([c, t * (c as Node3D).transform])
	if lights.is_empty() or pts.is_empty():
		return 0.0
	var worst := 0.0
	for p: Vector3 in lights:
		var best := INF
		for v in pts:
			best = minf(best, v.distance_squared_to(p))
		worst = maxf(worst, sqrt(best))
	return worst / maxf(ship.length, 0.001) * (1.0 / maxf(ship.scale.x, 0.001))


## Speck count over the hull's pixels of a frame (see the header). Returns {count, ppm, lit}.
func _specks(img: Image, bg: Image, skip: Array = []) -> Dictionary:
	var g := img.duplicate() as Image
	g.convert(Image.FORMAT_L8)
	var w := g.get_width()
	var h := g.get_height()
	var d := g.get_data()
	var b := bg.duplicate() as Image
	b.convert(Image.FORMAT_L8)
	var bd := b.get_data()
	var rgb := img.duplicate() as Image
	rgb.convert(Image.FORMAT_RGB8)
	var cd := rgb.get_data()
	var count := 0
	var lit := 0
	var hi := int(SPECK_LUMA * 255.0)
	var jump := int(SPECK_JUMP * 255.0)
	var r := SPECK_R
	var m := r + 1
	for y in range(m, h - m):
		var row := y * w
		for x in range(m, w - m):
			var v := d[row + x]
			if absi(int(v) - int(bd[row + x])) < 8:
				continue # sky, stars and dust: the same without the ship
			if not skip.is_empty() and _near(skip, x, y):
				continue # a navigation light: a light, measured by _nav_off, not hull sparkle
			lit += 1
			if v < hi:
				continue
			var ci := (row + x) * 3
			if cd[ci] < SPECK_WHITE or cd[ci + 1] < SPECK_WHITE or cd[ci + 2] < SPECK_WHITE:
				continue
			var lower := 0
			var thr := v - jump
			lower += 1 if d[row + x - r] < thr else 0
			lower += 1 if d[row + x + r] < thr else 0
			lower += 1 if d[row - r * w + x] < thr else 0
			lower += 1 if d[row + r * w + x] < thr else 0
			lower += 1 if d[row - r * w + x - r] < thr else 0
			lower += 1 if d[row - r * w + x + r] < thr else 0
			lower += 1 if d[row + r * w + x - r] < thr else 0
			lower += 1 if d[row + r * w + x + r] < thr else 0
			if lower >= 7:
				count += 1
	return {"count": count, "lit": lit, "ppm": float(count) / maxf(float(lit), 1.0) * 1.0e6}
