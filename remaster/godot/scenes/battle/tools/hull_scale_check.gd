extends SceneTree
## Hull scale check (M4 flight stage 3): opens a drill battle through the game's own screens and prints, for every ship
## on the battle screen, its class, the canon metres it is drawn at, its displayed length (world units), the measured
## length of the drawn model along its bow, and that model's metres (measured x 17.7). Off-screen, Dummy audio.
## args after --: <out_dir> <tactical|starbase|classic|fleetN>
var app: Node

func wait(k := 2) -> void:
	for i in k:
		await process_frame

func _find(node: Node, needle: String) -> Control:
	if node is Control and node.name == needle:
		return node as Control
	for c in node.get_children():
		var r := _find(c, needle)
		if r != null:
			return r
	return null

func _initialize() -> void:
	app = load("res://scenes/ui/app.tscn").instantiate()
	root.add_child(app)
	_run.call_deferred()

func _run() -> void:
	var a := OS.get_cmdline_user_args()
	var out: String = a[0]
	var mode: String = a[1]
	await wait(10)
	app.view = {}
	app.goto("title")
	await wait(6)
	if mode == "starbase":
		app.start_drill("newBattleDrill", "Starbase drill", {"station": true})
	elif mode.begins_with("fleet"):
		app.start_drill("newBattleDrill", "Tactical drill", {"ships": int(mode.trim_prefix("fleet"))})
	else:
		app.start_drill("newBattleDrill", "Tactical drill")
	await wait(4)
	app.end_turn()
	await wait(30)
	var take := _find(app, "TakeCommand")
	if take and take is BaseButton:
		take.emit_signal("pressed")
		app.close_modal()
	await wait(10)
	var cs: Node = app.current_screen()
	cs.order("Engage")
	await process_frame
	cs._round()
	await wait(30)
	var vn: Node = cs.host.view_node if cs.get("host") != null else null
	var lines := PackedStringArray()
	var m_per_wu := 1.0 / (BattleShip.UE_UNITS_PER_METER / BattleShip.UE_CELL_UNITS * BattleScene.CELL)
	lines.append("m_per_wu %.2f" % m_per_wu)
	var seen := {}
	if vn != null:
		for fid in vn._ships:
			var s: BattleShip = vn._ships[fid]
			var box := AABB()
			var any := false
			for mi in s.find_children("*", "MeshInstance3D", true, false):
				var m := mi as MeshInstance3D
				if m.mesh == null or not m.is_visible_in_tree():
					continue
				var bb := (s.global_transform.affine_inverse() * m.global_transform) * m.mesh.get_aabb()
				box = bb if not any else box.merge(bb)
				any = true
			var info: Dictionary = {}
			for es in vn.encounter.get("ships", []):
				if int(es.get("id", 0)) == fid:
					info = es
			var cls := str(info.get("className", info.get("shipClass", info.get("class", info.get("assetKey", "?")))))
			var key := "%s|%.0f|%s" % [str(s.hero_key), s.length_meters, cls]
			var line: String = "SHIP %d owner %d class '%s' name '%s' hero '%s' canon_m %.0f length_wu %.2f model_x_wu %.2f model_m %.0f station %s" % [
				fid, s.owner_id, cls, s.ship_name, s.hero_key, s.length_meters, s.length, box.size.x, box.size.x * m_per_wu, s.is_station]
			var mats := []
			for mi2 in s.find_children("*", "MeshInstance3D", true, false):
				var m2 := mi2 as MeshInstance3D
				if m2.mesh == null:
					continue
				for si in m2.mesh.get_surface_count():
					var sm := m2.get_surface_override_material(si) as ShaderMaterial
					if sm != null and mats.size() < 3:
						mats.append("%s tint=%s gain=%s em=%s" % [sm.shader.resource_path.get_file(), sm.get_shader_parameter("tint"), sm.get_shader_parameter("color_gain"), sm.get_shader_parameter("emissive_strength")])
			line += " MATS " + " ; ".join(mats)
			if not seen.has(key):
				seen[key] = true
				lines.append(line)
	for l in lines:
		print(l)
	var f := FileAccess.open(out.path_join("hull-scale.txt"), FileAccess.WRITE)
	f.store_string("\n".join(lines) + "\n")
	f.close()
	quit()
