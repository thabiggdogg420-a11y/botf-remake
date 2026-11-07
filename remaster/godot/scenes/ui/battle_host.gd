class_name BattleHost
extends Control
## Host container for the tactical battle view owned by scenes/battle (3D, another team).
##
## Integration contract (the battle scene's root node implements these; all optional except the first):
##   func load_encounter(encounter: Dictionary) -> void      # full `tactical` / `replay` Encounter JSON
##   func play_round(round: Dictionary, encounter: Dictionary) -> void   # animate one TacticalRound (rules 3 events)
##   func set_selection(ship_id: int, target_id: int) -> void
##   signal ship_picked(ship_id: int)
##
## Until a scene exists at BATTLE_SCENE, a 2D placeholder draws the arena from ship positions.

signal ship_picked(ship_id: int)

const BATTLE_SCENE := "res://scenes/battle/battle_view.tscn"

var view_node: Node = null
var placeholder: BattlePlaceholder = null

func _ready() -> void:
	name = "BattleHost"
	clip_contents = true
	if ResourceLoader.exists(BATTLE_SCENE):
		var packed: PackedScene = load(BATTLE_SCENE)
		attach(packed.instantiate())
	else:
		placeholder = BattlePlaceholder.new()
		placeholder.set_anchors_preset(Control.PRESET_FULL_RECT)
		placeholder.picked.connect(func(id): ship_picked.emit(id))
		add_child(placeholder)

## Mounts a battle view (Control, or a Node3D which is wrapped in a SubViewport).
func attach(node: Node) -> void:
	if placeholder:
		placeholder.queue_free()
		placeholder = null
	if node is Node3D:
		var svc := SubViewportContainer.new()
		svc.stretch = true
		svc.set_anchors_preset(Control.PRESET_FULL_RECT)
		var sv := SubViewport.new()
		sv.own_world_3d = true
		sv.add_child(node)
		svc.add_child(sv)
		add_child(svc)
	else:
		if node is Control:
			(node as Control).set_anchors_preset(Control.PRESET_FULL_RECT)
		add_child(node)
	view_node = node
	if node.has_signal("ship_picked"):
		node.connect("ship_picked", func(id): ship_picked.emit(id))
	_add_camera_toggle.call_deferred()

## A small "Camera: cinematic / free · C" button over the 3D view. Free orbit starts from the cinematic framing, so
## without it the C toggle looks like it did nothing.
func _add_camera_toggle() -> void:
	if view_node == null or not ("camera" in view_node) or view_node.camera == null:
		return
	var cam: Object = view_node.camera
	var b := Button.new()
	b.name = "CameraMode"
	b.theme_type_variation = "GhostButton"
	b.focus_mode = Control.FOCUS_NONE
	b.position = Vector2(12, 12)
	b.tooltip_text = "Cinematic follows the action; free camera: drag to rotate, wheel to zoom, right-drag to pan."
	var label := func(m: int) -> String:
		return "CAMERA: %s  ·  C" % ("CINEMATIC" if m == 0 else "FREE")
	b.text = label.call(int(cam.mode))
	b.pressed.connect(func(): cam.set_mode(1 - int(cam.mode)))
	cam.mode_changed.connect(func(m): b.text = label.call(m))
	add_child(b)

func load_encounter(e: Dictionary) -> void:
	if view_node and view_node.has_method("load_encounter"):
		view_node.load_encounter(e)
	elif placeholder:
		placeholder.encounter = e
		placeholder.shown_round = e.get("rounds", []).size()
		placeholder.queue_redraw()

func play_round(r: Dictionary, e: Dictionary) -> void:
	if view_node and view_node.has_method("play_round"):
		view_node.play_round(r, e)
	elif placeholder:
		placeholder.encounter = e
		placeholder.shown_round = int(r.get("index", e.get("rounds", []).size()))
		placeholder.flash = 1.0
		placeholder.queue_redraw()

func set_selection(ship: int, target: int) -> void:
	if view_node and view_node.has_method("set_selection"):
		view_node.set_selection(ship, target)
	elif placeholder:
		placeholder.selected = ship
		placeholder.target = target
		placeholder.queue_redraw()

## 2D stand-in: ships at their arena coordinates, hull/shield rings, next-order intent lines,
## and the last round's salvos.
class BattlePlaceholder extends Control:
	signal picked(id: int)
	var encounter: Dictionary = {}
	var shown_round := 0
	var selected := 0
	var target := 0
	var flash := 0.0

	func _process(delta: float) -> void:
		if flash > 0.0:
			flash = max(0.0, flash - delta * 0.8)
		queue_redraw()

	func _arena_to_screen(p: Dictionary) -> Vector2:
		var w := float(encounter.get("arenaWidth", 100))
		var h := float(encounter.get("arenaHeight", 100))
		var m := 60.0
		return Vector2(m + float(p["x"]) / max(1.0, w) * (size.x - 2 * m), m + float(p["y"]) / max(1.0, h) * (size.y - 2 * m))

	func _ship_pos(s: Dictionary) -> Variant:
		var rounds: Array = encounter.get("rounds", [])
		if shown_round > 0 and shown_round <= rounds.size():
			for rs in rounds[shown_round - 1]["ships"]:
				if int(rs["id"]) == int(s["id"]):
					return _arena_to_screen(rs["to"])
		if s.get("position") is Dictionary:
			return _arena_to_screen(s["position"])
		return null

	func _gui_input(ev: InputEvent) -> void:
		if ev is InputEventMouseButton and ev.pressed and ev.button_index == MOUSE_BUTTON_LEFT:
			var best := 0
			var bd := 40.0
			for s in encounter.get("ships", []):
				var p: Variant = _ship_pos(s)
				if p is Vector2 and (p as Vector2).distance_to(ev.position) < bd:
					bd = (p as Vector2).distance_to(ev.position)
					best = int(s["id"])
			if best != 0:
				picked.emit(best)

	func _draw() -> void:
		var t := Time.get_ticks_msec() / 1000.0
		draw_rect(Rect2(Vector2.ZERO, size), Color(0.01, 0.015, 0.035, 1.0))
		for k in range(0, int(size.x), 48):
			draw_line(Vector2(k, 0), Vector2(k, size.y), Color(0.4, 0.55, 0.9, 0.05))
		for k in range(0, int(size.y), 48):
			draw_line(Vector2(0, k), Vector2(size.x, k), Color(0.4, 0.55, 0.9, 0.05))
		var f := Lcars.font("display", 560)
		draw_string(f, Vector2(18, 30), "BATTLE HOST · TACTICAL VIEW PLACEHOLDER", HORIZONTAL_ALIGNMENT_LEFT, -1, 16, Color(Lcars.TEXT3, 0.8))
		draw_string(Lcars.font("mono"), Vector2(18, 50), "3D battle scene mounts here (scenes/battle)", HORIZONTAL_ALIGNMENT_LEFT, -1, 12, Color(Lcars.TEXT3, 0.6))
		if encounter.is_empty():
			return
		var ships: Array = encounter.get("ships", [])
		var pos := {}
		for s in ships:
			var p: Variant = _ship_pos(s)
			if p is Vector2:
				pos[int(s["id"])] = p
		# Salvos of the shown round
		var rounds: Array = encounter.get("rounds", [])
		if shown_round > 0 and shown_round <= rounds.size():
			for sv in rounds[shown_round - 1].get("salvos", []):
				if pos.has(int(sv["from"])) and pos.has(int(sv["to"])):
					var a: Vector2 = pos[int(sv["from"])]
					var b: Vector2 = pos[int(sv["to"])]
					var col := Color("#ff9a4a") if int(sv.get("torpedo", 0)) > int(sv.get("beam", 0)) else Color("#ffcf6a")
					if sv.get("ram", false):
						col = Lcars.ALERT
					draw_line(a, b, Color(col, 0.25 + 0.6 * flash), 2.0 + 2.0 * flash, true)
		# Intent lines
		for s in ships:
			var nx: Variant = s.get("next")
			if nx is Dictionary and int(nx.get("target", 0)) != 0 and pos.has(int(s["id"])) and pos.has(int(nx["target"])):
				var a2: Vector2 = pos[int(s["id"])]
				var b2: Vector2 = pos[int(nx["target"])]
				var dir := (b2 - a2)
				var n := int(dir.length() / 14.0)
				for k in range(n):
					if k % 2 == 0:
						draw_line(a2 + dir * (float(k) / n), a2 + dir * (float(k + 1) / n), Color(Lcars.owner_color(int(s["owner"])), 0.35), 1.0)
		for s in ships:
			if not pos.has(int(s["id"])):
				continue
			var p2: Vector2 = pos[int(s["id"])]
			var c := Lcars.owner_color(int(s["owner"]))
			var alive: bool = s.get("alive", true) and not s.get("retreated", false)
			if s.get("hidden", false):
				draw_arc(p2, 12, 0, TAU, 20, Color(c, 0.3), 1.0, true)
				continue
			var hull_f: float = float(s["hull"]) / max(1.0, float(s["maxHull"]))
			var sh_f: float = float(s["shields"]) / max(1.0, float(s["maxShields"]))
			var dir2 := Vector2(1, 0) if s.get("mine", false) else Vector2(-1, 0)
			var side := Vector2(0, 1)
			var body := PackedVector2Array([p2 + dir2 * 14, p2 - dir2 * 10 + side * 9, p2 - dir2 * 5, p2 - dir2 * 10 - side * 9])
			if s.get("station", false):
				body = PackedVector2Array()
				for k in range(6):
					body.append(p2 + Vector2(cos(k * TAU / 6.0), sin(k * TAU / 6.0)) * 14)
			draw_colored_polygon(body, Color(c, 0.9 if alive else 0.25))
			if alive:
				draw_arc(p2, 20, -PI / 2, -PI / 2 + TAU * sh_f, 32, Color(Lcars.SHIELD, 0.8), 2.0, true)
				draw_arc(p2, 24, -PI / 2, -PI / 2 + TAU * hull_f, 32, Color("#8fd18a"), 2.0, true)
				if s.get("cloaked", false):
					draw_arc(p2, 28, t, t + PI, 20, Color(1, 1, 1, 0.4), 1.0, true)
			else:
				draw_line(p2 - Vector2(10, 10), p2 + Vector2(10, 10), Lcars.ALERT, 2.0)
				draw_line(p2 - Vector2(10, -10), p2 + Vector2(10, -10), Lcars.ALERT, 2.0)
			if int(s["id"]) == selected:
				draw_arc(p2, 32 + 2 * sin(t * 5), 0, TAU, 32, Color.WHITE, 2.0, true)
			if int(s["id"]) == target:
				draw_arc(p2, 32, 0, TAU, 32, Lcars.ALERT, 2.0, true)
			var nm := str(s["name"]).split(" — ")[0]
			draw_string(f, p2 + Vector2(-40, 44), nm.to_upper(), HORIZONTAL_ALIGNMENT_CENTER, 80, 12, Color(Lcars.TEXT2, 0.9 if alive else 0.4))
