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
## The scene at BATTLE_SCENE always ships with the project (the 2D BattlePlaceholder that stood in for it was removed
## in R3); if it is ever missing the host is an empty panel and says so in the log.

signal ship_picked(ship_id: int)

const BATTLE_SCENE := "res://scenes/battle/battle_view.tscn"

var view_node: Node = null

func _ready() -> void:
	name = "BattleHost"
	clip_contents = true
	if ResourceLoader.exists(BATTLE_SCENE):
		var packed: PackedScene = load(BATTLE_SCENE)
		attach(packed.instantiate())
	else:
		push_error("BattleHost: %s is missing" % BATTLE_SCENE)

## Mounts a battle view (Control, or a Node3D which is wrapped in a SubViewport).
func attach(node: Node) -> void:
	if node is Node3D:
		var svc := SubViewportContainer.new()
		svc.stretch = true
		svc.set_anchors_preset(Control.PRESET_FULL_RECT)
		var sv := SubViewport.new()
		sv.own_world_3d = true
		if App.i != null:
			DisplaySettings.apply_viewport(sv, App.i.settings) # render scale, anti-aliasing, shadow atlas
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
	_add_camera_bar.call_deferred()

## The camera mode bar over the 3D view (as the Unreal combat screen): OVERVIEW CHASE SHIP TARGET TOP-DOWN CINEMATIC,
## then RESET (Home). The lit button follows the camera, whichever way its mode changed (keys, double-click, pan).
func _add_camera_bar() -> void:
	if view_node == null or not ("camera" in view_node) or view_node.camera == null or has_node("CameraBar"):
		return
	var cam: BattleCamera = view_node.camera
	var bar := HBoxContainer.new()
	bar.name = "CameraBar"
	bar.add_theme_constant_override("separation", 2)
	bar.position = Vector2(10, 10)
	bar.mouse_filter = Control.MOUSE_FILTER_IGNORE
	var group := ButtonGroup.new()
	var buttons := []
	for m in BattleCamera.MODE_NAMES.size():
		var b := _camera_button(BattleCamera.MODE_NAMES[m], BattleCamera.MODE_HINTS[m])
		b.name = "Cam" + BattleCamera.MODE_NAMES[m].replace("-", "")
		b.toggle_mode = true
		b.button_group = group
		b.button_pressed = int(cam.mode) == m
		b.pressed.connect(func(): cam.set_mode(m))
		bar.add_child(b)
		buttons.append(b)
	var sep := Control.new()
	sep.custom_minimum_size = Vector2(8, 0)
	sep.mouse_filter = Control.MOUSE_FILTER_IGNORE
	bar.add_child(sep)
	var reset := _camera_button("RESET  ·  HOME", "Frame the whole battle. Wheel zooms toward the cursor, left-drag orbits, right-drag pans; click a ship to select it, double-click to follow it or fly to empty space. F / G follow ship / target, +/- zoom, C cinematic.")
	reset.name = "CamReset"
	reset.pressed.connect(func(): cam.frame_battle())
	bar.add_child(reset)
	cam.mode_changed.connect(func(m):
		for i in buttons.size():
			buttons[i].set_pressed_no_signal(i == m))
	add_child(bar)


func _camera_button(text: String, tip: String) -> Button:
	var b := Button.new()
	b.text = text
	b.tooltip_text = tip
	b.theme_type_variation = "GhostButton"
	b.focus_mode = Control.FOCUS_NONE # keys stay with the battle view
	b.add_theme_font_size_override("font_size", 11)
	b.custom_minimum_size = Vector2(0, 24)
	return b

func load_encounter(e: Dictionary) -> void:
	if view_node and view_node.has_method("load_encounter"):
		view_node.load_encounter(e)

func play_round(r: Dictionary, e: Dictionary) -> void:
	if view_node and view_node.has_method("play_round"):
		view_node.play_round(r, e)

func set_selection(ship: int, target: int) -> void:
	if view_node and view_node.has_method("set_selection"):
		view_node.set_selection(ship, target)
