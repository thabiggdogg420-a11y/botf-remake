extends SceneTree
## Tactical rules 4 presentation: plan-phase overlays (arcs, range bands, move preview and ghost, forecast readout,
## subsystem picking), facing-aware shield hits, persistent subsystem damage and deck layers. With the native core it
## also drives the bridge's combat sandbox end to end (forecast -> aimed orders -> round -> events with facings).
##   godot --headless --path remaster/godot -s res://scenes/battle/tests/test_battle_plan.gd

const FIXTURE := "res://scenes/battle/fixtures/rules3_battle.json"

var failures := 0
var checks := 0


func check(cond: bool, what: String) -> void:
	checks += 1
	if not cond:
		failures += 1
		printerr("  FAIL  ", what)


func _initialize() -> void:
	_run.call_deferred()


func _run() -> void:
	print("Battle plan tests")
	var encounter: Dictionary = JSON.parse_string(FileAccess.get_file_as_string(FIXTURE))
	var scene: BattleScene = load("res://scenes/battle/battle_view.tscn").instantiate()
	scene.play_audio = false
	root.add_child(scene)
	await process_frame
	scene.play_round(encounter, 0)
	for i in 5:
		await process_frame
	var me: Dictionary = {}
	var foe: Dictionary = {}
	for s in encounter.ships:
		if s.get("mine", false) and me.is_empty():
			me = s
		elif not s.get("mine", false) and foe.is_empty():
			foe = s
	me = me.duplicate(true)
	me["weapons"] = [
		{"type": "PhaserArray", "family": "beam", "arc": "All", "arcCentre": 0, "arcHalfWidth": 180, "maxRange": 4},
		{"type": "PhotonTorpedo", "family": "torpedo", "arc": "ForwardWide", "arcCentre": 0, "arcHalfWidth": 90, "maxRange": 7},
		{"type": "PhaseCannon", "family": "pulse", "arc": "Broadside", "arcCentre": 90, "arcHalfWidth": 45, "maxRange": 3}]
	var p0: Dictionary = me.get("position", {"x": 3, "y": 5})
	var forecast := {"fleetId": me.id, "target": foe.id, "maneuver": "Engage", "subsystem": "engines", "exact": true,
		"path": [{"x": p0.x, "y": p0.y, "facing": 0}, {"x": p0.x + 1, "y": p0.y, "facing": 0}, {"x": p0.x + 2, "y": p0.y, "facing": 45}],
		"end": {"x": p0.x + 2, "y": p0.y, "facing": 45}, "expectedDamage": 57, "mountsFiring": 2,
		"mounts": [{"mount": 0, "weapon": "PhaserArray", "hitChance": 82, "damage": 40, "expectedDamage": 32, "facing": "fore", "facingShields": 120},
			{"mount": 1, "weapon": "PhotonTorpedo", "hitChance": 70, "damage": 36, "expectedDamage": 25, "facing": "fore", "facingShields": 120},
			{"mount": 2, "weapon": "PhaseCannon", "hitChance": 0, "damage": 0, "expectedDamage": 0, "facing": "none", "facingShields": 0}]}
	scene.show_plan(me, forecast)
	await process_frame
	var plan: BattlePlan = scene.plan
	check(plan.is_showing() and plan.target_id == int(foe.id), "plan shows for the selected ship and its target")
	var lines: ImmediateMesh = plan.get_node("PlanLines").mesh
	var fill: ImmediateMesh = plan.get_node("PlanFill").mesh
	check(lines.get_surface_count() == 1 and fill.get_surface_count() == 1, "arc wedges, range bands and the path are drawn")
	check(plan.get_node_or_null("Ghost") != null, "ghost hull at the end pose")
	var ghost: Node3D = plan.get_node("Ghost")
	var want := scene.cell_to_world(float(p0.x) + 2.0, float(p0.y))
	check(Vector2(ghost.global_position.x, ghost.global_position.z).distance_to(Vector2(want.x, want.z)) < 0.01, "ghost sits on the end cell")
	var readout: Label = plan.get_node("PlanUI/Overlay/Readout")
	check(readout.visible and readout.text.contains("PHASERARRAY  82%") and readout.text.contains("AIM ENGINES") and
		not readout.text.contains("PHASECANNON"), "forecast readout lists the mounts that bear: " + readout.text)
	check(BattlePlan.readout_text({"mounts": []}).contains("NO WEAPON BEARS"), "an empty forecast says so")
	# Subsystem picking: a tag's own screen point picks it; picking it again clears the aim.
	var cam: Camera3D = scene.camera
	var picked := []
	scene.subsystem_picked.connect(func(t, k): picked.append([t, k]))
	var pip: Label = plan.get_node("PlanUI/Overlay/Pip_warpCore")
	check(pip.visible, "subsystem tags on the target")
	# The tags are one vertical list inside the view, clear of each other and of the forecast panel.
	var vp := scene.get_viewport().get_visible_rect()
	var rows := []
	for key in BattlePlan.SUBSYSTEMS:
		rows.append(plan.tag_rect(key))
	var tidy := true
	for i in rows.size():
		tidy = tidy and (rows[i] as Rect2).size != Vector2.ZERO and vp.encloses(rows[i])
		if i > 0:
			tidy = tidy and absf(rows[i].position.x - rows[0].position.x) < 0.01 and rows[i].position.y >= rows[i - 1].end.y - 0.01
	check(tidy, "tags are a vertical list inside the view")
	check(vp.encloses(Rect2(readout.position, readout.size)), "forecast readout sits inside the view")
	check(not plan.tag_rect("weapons").intersects(Rect2(readout.position, readout.size)), "tags clear the forecast readout")
	var at: Vector2 = plan.tag_rect("warpCore").get_center()
	check(plan.subsystem_at(at, cam) == "warpCore", "a tag is under its own screen point")
	check(plan.pick(at, cam) and picked.size() == 1 and picked[0][1] == "warpCore" and picked[0][0] == int(foe.id), "clicking a tag aims at that subsystem")
	check(plan.subsystem_at(Vector2(-500, -500), cam) == "", "nothing picked off the pips")
	scene.clear_plan()
	check(not plan.is_showing() and plan.get_node_or_null("Ghost") == null or not plan.visible, "plan clears")

	# Facing-aware shield hits and persistent subsystem damage.
	var node: BattleShip = scene.ship_node(int(foe.id))
	node.max_facings = [100, 80, 90, 90, 60, 60]
	node.facings_after = [10, 80, 90, 90, 60, 60]
	check(is_equal_approx(node.facing_strength("fore"), 0.1) and is_equal_approx(node.facing_strength("bogus"), 1.0), "facing strength from the round state")
	node.add_shield_hit(node.global_position + Vector3(50, 0, 0), 0.0, 1.0, Color.CYAN, "starboard", 0.3)
	node.update_shield(0.2)
	var axes: Array = node._shield_mat.get_shader_parameter("hit_facing")
	check(axes.size() == 6 and (axes[0] as Vector4).is_equal_approx(Vector4(0, 0, 1, 0.3)), "a starboard hit lights the starboard segment")
	node.set_wego_state({"subsystems": {"weapons": 100, "engines": 0, "shields": 100, "sensors": 100, "warpCore": 40}, "coreBreached": false})
	check(node.system_effect_on("engines") and node.system_effect_on("warpCore") and not node.system_effect_on("weapons"), "dead engines and a failing core show")
	node.set_wego_state({"subsystems": {"weapons": 100, "engines": 60, "shields": 100, "sensors": 100, "warpCore": 0}, "coreBreached": true})
	check(not node.system_effect_on("engines") and node.system_effect_on("breach") and not node.system_effect_on("warpCore"), "repairs clear effects; a breach burns")
	check(node.subsystem_point("engines").distance_to(node.global_position) > 0.0, "subsystem points sit on the hull")

	# Timeline: rules 4 fields reach the impact jobs; deck layers set the flight height.
	var ev := [{"tick": 0, "kind": "ManeuverStart", "fleetId": 1, "x": 2, "y": 2, "facing": 0, "maneuver": "Engage"},
		{"tick": 4, "kind": "ManeuverEnd", "fleetId": 1, "x": 3, "y": 2, "facing": 0, "maneuver": "Engage", "value": 0},
		{"tick": 5, "kind": "Fire", "fleetId": 1, "targetFleetId": 2, "mountIndex": 0, "weapon": "PhaserArray", "family": "beam",
			"projectiles": 1, "hit": true, "intercepted": false, "impactTick": 5, "shieldDamage": 10, "hullDamage": 5, "destroyed": false,
			"x": 3, "y": 2, "facing": 0, "shieldFacing": "dorsal", "subsystem": "engines", "subsystemDamage": 30, "subsystemDestroyed": true, "breachDamage": 0, "aimed": true}]
	var info := [{"fleetId": 1, "ownerId": 1, "x": 2.0, "y": 2.0, "facing": 0.0, "lengthMeters": 300.0, "elevation": 1},
		{"fleetId": 2, "ownerId": 2, "x": 6.0, "y": 2.0, "facing": 180.0, "lengthMeters": 300.0, "elevation": -1}]
	var tl := BattleTimeline.build(ev, info)
	var shield_job := {}
	var hull_job := {}
	for j in tl.jobs:
		if j.kind == BattleTimeline.Job.SHIELD: shield_job = j
		if j.kind == BattleTimeline.Job.HULL: hull_job = j
	check(shield_job.get("facing", "") == "dorsal" and hull_job.get("subsystem_destroyed", false) and hull_job.get("subsystem", "") == "engines",
		"impact jobs carry the facing and the subsystem kill")
	check(tl.track(1).initial.altitude > 1.0 and tl.track(2).initial.altitude < -1.0, "deck layers: +1 above the plane, -1 below")

	# End to end with the native core: the combat sandbox.
	if Engine.has_singleton("BotfCore"):
		var core := Engine.get_singleton("BotfCore")
		var e: Dictionary = core.request("combatSandbox", {"ships": 3}).result
		check(int(e.get("rules", 0)) == 4 and e.ships[0].has("wego") and e.ships[0].wego.facings.size() == 6, "sandbox is a rules 4 encounter")
		var mine: Dictionary = {}
		var enemy := 0
		for s in e.ships:
			if s.mine and mine.is_empty(): mine = s
			elif not s.mine and enemy == 0: enemy = int(s.id)
		check(mine.weapons[0].has("arcHalfWidth") and mine.weapons[0].has("maxRange"), "weapons carry arc geometry and range")
		var f: Dictionary = core.request("tacticalForecast", {"sandbox": true, "fleetId": mine.id, "targetFleetId": enemy, "maneuver": "Charge", "subsystem": "weapons"}).result
		check(f.exact and f.path.size() >= 1 and f.mounts.size() == mine.weapons.size() and f.subsystem == "weapons", "forecast: exact path and one row per mount")
		core.request("tacticalOrder", {"sandbox": true, "fleetId": mine.id, "targetFleetId": enemy, "maneuver": "Charge", "subsystem": "weapons"})
		var r: Dictionary = core.request("tacticalRound", {"sandbox": true}).result
		var moved: Dictionary = r.rounds[0].ships.filter(func(x): return int(x.id) == int(mine.id))[0]
		check(int(moved.to.x) == int(f.end.x) and int(moved.to.y) == int(f.end.y), "the ship ends where the preview said")
		check(moved.subsystem == "weapons" and r.rounds[0].wego.size() == r.ships.size(), "round records the aim and every ship's state")
		var faced := 0
		for n in 6:
			for x in r.rounds[-1].events:
				if x.kind == "Fire" and x.get("hit", false) and x.has("shieldFacing"):
					faced += 1
			if faced > 0 or r.finished:
				break
			r = core.request("tacticalRound", {"sandbox": true}).result
		check(faced > 0, "hits name the facing struck")
		var bad: Dictionary = core.request("tacticalOrder", {"sandbox": true, "fleetId": mine.id, "maneuver": "Engage", "subsystem": "bridge"})
		check(not bad.ok, "unknown subsystem rejected")
	else:
		print("  (BotfCore not loaded: sandbox checks skipped)")
	print("%d checks, %d failures" % [checks, failures])
	quit(1 if failures > 0 else 0)
