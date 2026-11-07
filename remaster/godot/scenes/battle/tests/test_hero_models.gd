extends SceneTree
## Headless test of the hero-model path (blender_hero_ships.py --gltf -> godot-hero-manifest.json -> BattleShip):
## builds two tiny synthetic glbs from Godot primitives with GLTFDocument (a plated hull with a vertex colour and LOD1,
## a textured hull with BC / emissive maps), writes a manifest like the Blender tool's, and checks lookup, canon-length
## scaling, WPN_* sockets as weapon origins, the hero materials, LOD ranges and the procedural fallback.
##   godot --headless --path remaster/godot -s res://scenes/battle/tests/test_hero_models.gd

var failures := 0
var dir := ""


func check(cond: bool, what: String) -> void:
	if not cond:
		failures += 1
		printerr("  FAIL  ", what)


func near(a: Vector3, b: Vector3, eps := 1e-3) -> bool:
	return a.distance_to(b) <= eps


func _initialize() -> void:
	_run.call_deferred()


func _run() -> void:
	print("Hero model tests")
	dir = ProjectSettings.globalize_path("user://test_hero_models")
	DirAccess.make_dir_recursive_absolute(dir.path_join("ship.kli.testplated"))
	DirAccess.make_dir_recursive_absolute(dir.path_join("ship.fed.testtextured"))
	var plated_glb := dir.path_join("ship.kli.testplated/SM_testplated.glb")
	var textured_glb := dir.path_join("ship.fed.testtextured/SM_testtextured.glb")
	check(_write_glb(_plated_scene(), plated_glb) == OK, "plated glb written")
	check(_write_glb(_textured_scene(), textured_glb) == OK, "textured glb written")
	var manifest := {"version": 1, "heroes": {
		"ship.kli.testplated": {"glb": "ship.kli.testplated/SM_testplated.glb", "faction": "kli", "lengthMeters": 100.0,
			"sockets": ["WPN_Torp_01", "WPN_Beam_01", "ENG_01"], "socketPrefix": "SOCKET_", "material": "plated",
			"lods": [{"node": "SM_testplated", "ratio": 1.0, "screen": 1.0}, {"node": "SM_testplated_LOD1", "ratio": 0.5, "screen": 0.35}],
			"slots": [{"name": "M00", "kind": "plated", "faction": "kli", "vertexColor": "Col", "tint": null,
				"panelScale": 0.35, "tone": [0.7, 0.65], "seam": 0.55, "roughnessMin": 0.32, "metallic": 0.1575}]},
		"ship.fed.testtextured": {"glb": "ship.fed.testtextured/SM_testtextured.glb", "faction": "fed", "lengthMeters": 200.0,
			"sockets": ["WPN_Beam_01"], "socketPrefix": "SOCKET_", "material": "textured",
			"lods": [{"node": "SM_testtextured", "ratio": 1.0, "screen": 1.0}],
			"slots": [{"name": "M00", "kind": "textured", "tint": [1, 1, 1], "metallicScale": 0.35, "roughnessScale": 1.0,
				"roughnessMin": 0.32, "emissiveStrength": 0.75}]},
		"ship.fed.missing": {"glb": "ship.fed.missing/SM_missing.glb", "lengthMeters": 50.0, "slots": []},
	}}
	var mpath := dir.path_join("godot-hero-manifest.json")
	var f := FileAccess.open(mpath, FileAccess.WRITE)
	f.store_string(JSON.stringify(manifest, " "))
	f.close()

	# Lookup: the manifest is found through $BOTF_HERO_MANIFEST, and a key whose glb is absent is no hero.
	OS.set_environment("BOTF_HERO_MANIFEST", mpath)
	var assets := BattleAssets.new()
	OS.set_environment("BOTF_HERO_MANIFEST", "")
	check(assets.hero_manifest_path == mpath, "hero manifest found via BOTF_HERO_MANIFEST")
	check(not assets.hero("ship.kli.testplated").is_empty(), "plated hero resolves")
	check(assets.hero("ship.fed.missing").is_empty(), "hero without a glb on disk is skipped")
	check(is_equal_approx(assets.hero_length(PackedStringArray(["ship.fed.missing", "ship.fed.testtextured"])), 200.0), "hero_length")
	check(BattleShip.socket_name("SOCKET_WPN_Beam_01") == "WPN_BEAM_01" and BattleShip.socket_name("WPN_Torp") == "WPN_TORP", "socket_name")

	# Plated hero: canon 100 m becomes `length` world units; sockets scale with it.
	var holder := Node3D.new()
	root.add_child(holder)
	var ship := BattleShip.new()
	holder.add_child(ship)
	ship.setup({"id": 11, "owner": 2, "name": "Test", "assetKey": "ship.kli.testplated", "displayLength": 8.0}, assets)
	await process_frame
	check(ship.imported and ship.hero_key == "ship.kli.testplated", "plated ship uses the hero glb")
	check(is_equal_approx(ship.length, 8.0) and is_equal_approx(ship.canon_length_m, 100.0), "length 8 from canon 100 m")
	check(is_equal_approx(ship.model_scale, 0.08), "model scale 0.08 (got %f)" % ship.model_scale)
	check(absf(ship.half_extents.x - 4.0) < 1e-3, "hull spans the length (half %f)" % ship.half_extents.x)
	check(ship.sockets.size() == 2 and ship.engine_sockets.size() == 1, "2 WPN + 1 ENG sockets (%d, %d)" % [ship.sockets.size(), ship.engine_sockets.size()])
	ship.global_transform = Transform3D(Basis(Vector3.UP, 0.7), Vector3(30, 2, -5))
	# Torpedo socket at +48 m on the bow, beam socket at (10, 9, 0) m: 0.08 units per metre, on a centred hull.
	var torp := ship.hardpoint(2, 0, 0)
	check(near(torp, ship.global_transform * Vector3(3.84, 0, 0)), "torpedo origin at the WPN_Torp socket (%s)" % torp)
	var beam := ship.hardpoint(0, 0, 0)
	check(near(beam, ship.global_transform * Vector3(0.8, 0.72, 0)), "beam origin at the WPN_Beam socket (%s)" % beam)
	var mi := ship.find_child("SM_testplated", true, false) as MeshInstance3D
	check(mi != null, "LOD0 mesh present")
	if mi != null:
		var mat := mi.get_surface_override_material(0) as ShaderMaterial
		check(mat != null and mat.shader == HeroLook.PLATED_SHADER, "plated hull material")
		check(mat != null and bool(mat.get_shader_parameter("use_vertex_color")), "plated material reads the vertex colour")
		check(mat != null and is_equal_approx(float(mat.get_shader_parameter("metallic")), 0.1575), "plated metallic from the hint")
		var arrays := mi.mesh.surface_get_arrays(0)
		var cols = arrays[Mesh.ARRAY_COLOR]
		check(cols is PackedColorArray and cols.size() > 0 and _close(cols[0], Color(0.19, 0.25, 0.16)), "vertex colour Col survives the glb")
		check(mi.visibility_range_begin == 0.0 and mi.visibility_range_end > 0.0, "LOD0 range ends")
	var lod1 := ship.find_child("SM_testplated_LOD1", true, false) as MeshInstance3D
	check(lod1 != null and lod1.visibility_range_begin > 0.0 and lod1.visibility_range_end == 0.0, "LOD1 range begins")
	if mi != null and lod1 != null:
		check(is_equal_approx(mi.visibility_range_end, lod1.visibility_range_begin), "LOD ranges meet")
		check(is_equal_approx(lod1.visibility_range_begin, HeroLook.lod_distance(8.0, 0.35)), "LOD1 distance from the screen size")

	# Textured hero.
	var ship2 := BattleShip.new()
	holder.add_child(ship2)
	ship2.setup({"id": 12, "owner": 1, "name": "Test", "assetKey": "ship.fed.testtextured", "displayLength": 10.0}, assets)
	check(ship2.imported and is_equal_approx(ship2.model_scale, 0.05), "textured hero scaled by canon 200 m")
	var mi2 := ship2.find_child("SM_testtextured", true, false) as MeshInstance3D
	if mi2 != null:
		var mat2 := mi2.get_surface_override_material(0) as ShaderMaterial
		check(mat2 != null and mat2.shader == HeroLook.HULL_SHADER, "textured hull material")
		if mat2 != null:
			check(mat2.get_shader_parameter("bc_tex") != null, "BC texture carried over from the glb")
			check(mat2.get_shader_parameter("emissive_tex") != null, "emissive texture carried over")
			check(is_equal_approx(float(mat2.get_shader_parameter("emissive_strength")), 0.75), "emissive strength from the hint")
			check(is_equal_approx(float(mat2.get_shader_parameter("metallic_scale")), 0.35), "metallic scale 0.35")
			check(is_equal_approx(float(mat2.get_shader_parameter("roughness_min")), 0.32), "roughness floor 0.32")
		check(mi2.visibility_range_end == 0.0, "single-LOD hero is never hidden")
	else:
		check(false, "textured mesh present")
	check(ship2.sockets.size() == 1, "textured hero socket")

	# Fallbacks: a key with no hero (or a missing glb) builds the procedural hull.
	var ship3 := BattleShip.new()
	holder.add_child(ship3)
	ship3.setup({"id": 13, "owner": 1, "name": "Missing", "assetKey": "ship.fed.missing", "maxHull": 80}, assets)
	check(not ship3.imported and ship3.hero_key == "" and ship3.sockets.is_empty(), "missing glb falls back to the procedural hull")
	var ship4 := BattleShip.new()
	holder.add_child(ship4)
	ship4.setup({"id": 14, "owner": 3, "name": "Warbird", "maxHull": 120}, BattleAssets.new())
	check(not ship4.imported and ship4.find_child("Hull", true, false) != null, "no manifest: procedural hull")
	var fb := ship4.hardpoint(0, 0, 0)
	check(fb.is_finite(), "fallback hardpoint")

	holder.queue_free()
	await process_frame
	print("hero model tests: %d failures" % failures)
	quit(1 if failures > 0 else 0)


func _write_glb(scene: Node3D, path: String) -> Error:
	var doc := GLTFDocument.new()
	var state := GLTFState.new()
	var err := doc.append_from_scene(scene, state)
	if err == OK:
		err = doc.write_to_filesystem(state, path)
	scene.free()
	return err


## A 100 m hull along +X with vertex colour, UVs and one material "M00", as blender_hero_ships.py exports a plated hero.
func _box_mesh(size: Vector3, colour: Color, mat: Material) -> ArrayMesh:
	var box := BoxMesh.new()
	box.size = size
	var arrays := box.get_mesh_arrays()
	var cols := PackedColorArray()
	cols.resize((arrays[Mesh.ARRAY_VERTEX] as PackedVector3Array).size())
	cols.fill(colour)
	arrays[Mesh.ARRAY_COLOR] = cols
	var mesh := ArrayMesh.new()
	mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
	mesh.surface_set_material(0, mat)
	return mesh


func _socket(parent: Node3D, node_name: String, at: Vector3) -> void:
	var n := Node3D.new()
	n.name = node_name
	n.position = at
	parent.add_child(n)
	n.owner = parent.owner if parent.owner != null else parent


## Within 8-bit quantisation (the glb stores the colour as normalised bytes).
func _close(a: Color, b: Color) -> bool:
	return absf(a.r - b.r) < 0.006 and absf(a.g - b.g) < 0.006 and absf(a.b - b.b) < 0.006


func _plated_scene() -> Node3D:
	var scene := Node3D.new()
	scene.name = "testplated"
	var mat := StandardMaterial3D.new()
	mat.resource_name = "M00"
	mat.vertex_color_use_as_albedo = true
	var lod0 := MeshInstance3D.new()
	lod0.name = "SM_testplated"
	lod0.mesh = _box_mesh(Vector3(100, 20, 30), Color(0.19, 0.25, 0.16), mat)
	scene.add_child(lod0)
	lod0.owner = scene
	var lod1 := MeshInstance3D.new()
	lod1.name = "SM_testplated_LOD1"
	lod1.mesh = _box_mesh(Vector3(100, 20, 30), Color(0.19, 0.25, 0.16), mat)
	scene.add_child(lod1)
	lod1.owner = scene
	_socket(lod0, "SOCKET_WPN_Torp_01", Vector3(48, 0, 0))
	_socket(lod0, "SOCKET_WPN_Beam_01", Vector3(10, 9, 0))
	_socket(lod0, "SOCKET_ENG_01", Vector3(-50, 0, 0))
	return scene


func _textured_scene() -> Node3D:
	var scene := Node3D.new()
	scene.name = "testtextured"
	var img := Image.create(8, 8, false, Image.FORMAT_RGBA8)
	img.fill(Color(0.7, 0.7, 0.75))
	var glow := Image.create(8, 8, false, Image.FORMAT_RGBA8)
	glow.fill(Color(0, 0, 0))
	glow.set_pixel(2, 2, Color(1, 0.9, 0.6))
	var mat := StandardMaterial3D.new()
	mat.resource_name = "M00"
	mat.albedo_texture = ImageTexture.create_from_image(img)
	mat.emission_enabled = true
	mat.emission_texture = ImageTexture.create_from_image(glow)
	var mi := MeshInstance3D.new()
	mi.name = "SM_testtextured"
	mi.mesh = _box_mesh(Vector3(200, 30, 60), Color.WHITE, mat)
	scene.add_child(mi)
	mi.owner = scene
	_socket(mi, "SOCKET_WPN_Beam_01", Vector3(40, 15, 0))
	return scene
