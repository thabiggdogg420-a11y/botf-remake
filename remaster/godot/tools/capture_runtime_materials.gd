extends SceneTree
## WebGPU round 6 (19 Nov 2026), Barclay. Builds the scenes the browser build draws first (title fleet for every power,
## the fixture battle played for a few rounds, the galaxy map) and saves one copy of every distinct runtime material
## (StandardMaterial3D / ORMMaterial3D / ParticleProcessMaterial, keyed by the settings that pick their generated shader)
## to res://scenes/warm/bake/auto/. Those .tres files are exported, so the export's shader baker bakes their shaders, and
## ShaderWarm draws each one a frame at a time behind the "Starting" panel. Nothing in the game loads them otherwise.
##   godot --headless -s res://tools/capture_runtime_materials.gd
const OUT := "res://scenes/warm/bake/auto"
const FIXTURE := "res://scenes/battle/fixtures/rules3_battle.json"

var _seen := {}
var _n := 0
var _root: Node3D
var _frames := 0
var _view: Node
var _enc: Dictionary


func _initialize() -> void:
	DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path(OUT))
	_root = Node3D.new()
	root.add_child(_root)
	for e in range(1, 6):
		var tf := TitleFleet.new()
		_root.add_child(tf)
		tf.setup(e, null)
	var enc: Variant = JSON.parse_string(FileAccess.get_file_as_string(FIXTURE))
	if enc is Dictionary:
		_enc = enc
		_view = (load("res://scenes/battle/battle_view.tscn") as PackedScene).instantiate()
		if "play_audio" in _view:
			_view.play_audio = false
		_root.add_child(_view)
	var gm_scene := "res://scenes/galaxy/galaxy_map.tscn"
	if ResourceLoader.exists(gm_scene):
		_root.add_child((load(gm_scene) as PackedScene).instantiate())


func _process(_delta: float) -> bool:
	# r7: the effect materials BattleFx.prewarm() builds (every burst kind, spark stream) live only 3 frames, and the
	# title's background battle warm-up draws them on the web: capture them every frame while they exist.
	if _view != null and _frames == 5 and "fx" in _view and _view.fx != null:
		_view.fx.prewarm()
	if _view != null and _frames % 60 == 0 and _frames / 60 < 3 and _frames / 60 < (_enc.get("rounds", []) as Array).size():
		_view.play_round(_enc, _frames / 60)
	_frames += 1
	_collect(root) # every frame (r7): prewarm's bursts are freed after 3 frames, a 15-frame sample missed them
	if _frames >= 200:
		# r7: a damaged ship's fire, smoke and vent emitters (BattleShip._particle_mat) only exist once a hull is hit
		# hard enough; the first one compiled mid-round on the web. Every additive/flipbook combination.
		var tex := BattleShip.puff_texture()
		for additive in [false, true]:
			for flipbook in [false, true]:
				_take(BattleShip._particle_mat(tex, additive, flipbook))
		print("runtime materials: %d saved to %s" % [_n, OUT])
		return true
	return false


func _collect(n: Node) -> void:
	if n is GeometryInstance3D:
		_take(n.material_override)
		_take(n.material_overlay)
	if n is MeshInstance3D and n.mesh != null:
		for i in n.mesh.get_surface_count():
			_take(n.mesh.surface_get_material(i))
			_take(n.get_surface_override_material(i))
	if n is GPUParticles3D:
		_take(n.process_material)
		for p in n.draw_passes:
			var m: Mesh = n.get("draw_pass_%d" % (p + 1))
			_mesh(m)
	if n is CPUParticles3D:
		_mesh(n.mesh)
	if n is MultiMeshInstance3D and n.multimesh != null:
		_mesh(n.multimesh.mesh)
	for c in n.get_children():
		_collect(c)


func _mesh(m: Mesh) -> void:
	if m == null:
		return
	if m is PrimitiveMesh:
		_take((m as PrimitiveMesh).material)
	for i in m.get_surface_count():
		_take(m.surface_get_material(i))


func _take(m: Material) -> void:
	if m == null or not (m is BaseMaterial3D or m is ParticleProcessMaterial):
		return
	var key := _key(m)
	if _seen.has(key):
		return
	_seen[key] = true
	var d: Material = m.duplicate()
	for p in d.get_property_list():
		if p.type == TYPE_OBJECT and d.get(p.name) is Texture2D:
			var t: Texture2D = d.get(p.name)
			if t.resource_path == "" or t.resource_path.contains("::"):
				var img := Image.create(1, 1, false, Image.FORMAT_RGBA8)
				img.fill(Color.WHITE)
				d.set(p.name, ImageTexture.create_from_image(img))
	_n += 1
	var path := "%s/%s_%03d.tres" % [OUT, m.get_class().to_snake_case(), _n]
	if ResourceSaver.save(d, path) != OK:
		push_warning("could not save " + path)


## The settings that pick the generated shader: every bool, int and enum property, plus which texture slots are filled.
func _key(m: Material) -> String:
	var parts := [m.get_class()]
	for p in m.get_property_list():
		if not (p.usage & PROPERTY_USAGE_STORAGE):
			continue
		var v: Variant = m.get(p.name)
		match p.type:
			TYPE_BOOL, TYPE_INT:
				parts.append("%s=%s" % [p.name, v])
			TYPE_OBJECT:
				parts.append("%s=%s" % [p.name, "1" if v != null else "0"])
	return "|".join(parts)
