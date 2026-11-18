class_name BattleHost
extends Control
## Host container for the tactical battle view owned by scenes/battle (3D, another team).
##
## Integration contract (the battle scene's root node implements these; all optional except the first):
##   func load_encounter(encounter: Dictionary) -> void      # full `tactical` / `replay` Encounter JSON
##   func play_round(round: Dictionary, encounter: Dictionary) -> void   # animate one TacticalRound (rules 3 events)
##   func set_selection(ship_id: int, target_id: int) -> void
##   signal ship_picked(ship_id: int)
signal subsystem_picked(target_id: int, subsystem: String)
signal round_finished(round_index: int)
##   func show_plan(ship: Dictionary, forecast: Dictionary) -> void   # rules 4 plan overlay (tacticalForecast result)
##   func clear_plan() -> void
##   signal subsystem_picked(target_id: int, subsystem: String)
##   signal round_finished(round_index: int)
##
## The scene at BATTLE_SCENE always ships with the project (the 2D BattlePlaceholder that stood in for it was removed
## in R3); if it is ever missing the host is an empty panel and says so in the log.

signal ship_picked(ship_id: int)

const BATTLE_SCENE := "res://scenes/battle/battle_view.tscn"

var view_node: Node = null
var _last_encounter := {}

func _ready() -> void:
	name = "BattleHost"
	clip_contents = true
	if AppLoader.exists(BATTLE_SCENE):
		if DisplayServer.get_name() == "headless":
			_mount()
		else:
			# The 3D view (environment, effects, camera) is built on the frame after the screen, so no single frame
			# carries both (M1-BETAFIX: the first battle open cost 285 ms in one frame).
			_mount_next_frame()
	else:
		push_error("BattleHost: %s is missing" % BATTLE_SCENE)

func _mount_next_frame() -> void:
	await get_tree().process_frame
	if is_inside_tree():
		_mount()


func _mount() -> void:
	var t0 := Time.get_ticks_usec()
	var spare: Node = App.take_spare_battle_view() if App.i != null else null
	if spare == null:
		var packed: PackedScene = AppLoader.resource(BATTLE_SCENE)
		spare = packed.instantiate()
	attach(spare)
	var t1 := Time.get_ticks_usec()
	if not _last_encounter.is_empty():
		load_encounter(_last_encounter) # the screen asked before the view existed
	if OS.get_environment("ROUND_TIMING") != "":
		print("ROUND_TIMING mount frame %d: attach=%d load=%d ms" % [Engine.get_process_frames(), (t1 - t0) / 1000, (Time.get_ticks_usec() - t1) / 1000])


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
		# No CompatBloom here (WEB-PARITY.md, 19 Nov): the web hero hulls carry bright specular speckle that the 8-bit frame
		# clips to white, and the bloom spread it into a white haze over every hull. Back on once the hull speckle is fixed.
	else:
		if node is Control:
			(node as Control).set_anchors_preset(Control.PRESET_FULL_RECT)
		add_child(node)
	view_node = node
	if "threaded_build" in node:
		node.threaded_build = true # F2: round playback built off the main thread (no hitch at the start of a round)
	if node.has_signal("ship_picked"):
		node.connect("ship_picked", func(id): ship_picked.emit(id))
	if node.has_signal("subsystem_picked"):
		node.connect("subsystem_picked", func(t, k): subsystem_picked.emit(t, k))
	if node.has_signal("round_finished"):
		node.connect("round_finished", func(i): round_finished.emit(i))
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
	_add_playback_buttons(bar)
	cam.mode_changed.connect(func(m):
		for i in buttons.size():
			buttons[i].set_pressed_no_signal(i == m))
	add_child(bar)


## Round playback (M1 battleplay): 1x and 2x speed, and SKIP to the round's end (the ships stand where the core left
## them and the next orders open). The speed holds from round to round.
func _add_playback_buttons(bar: HBoxContainer) -> void:
	if not ("playback_speed" in view_node):
		return
	var sep := Control.new()
	sep.custom_minimum_size = Vector2(8, 0)
	sep.mouse_filter = Control.MOUSE_FILTER_IGNORE
	bar.add_child(sep)
	var group := ButtonGroup.new()
	for speed in [1.0, 2.0]:
		var b := _camera_button("%dx" % int(speed), "Play the round back at %dx speed." % int(speed))
		b.name = "Speed%d" % int(speed)
		b.toggle_mode = true
		b.button_group = group
		b.button_pressed = is_equal_approx(float(view_node.playback_speed), speed)
		b.pressed.connect(func(): view_node.playback_speed = speed)
		bar.add_child(b)
	if view_node.has_method("skip_round"):
		var skip := _camera_button("SKIP", "Skip to the end of this round's playback.")
		skip.name = "SkipRound"
		skip.pressed.connect(func(): view_node.skip_round())
		bar.add_child(skip)


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
	_last_encounter = e
	if view_node and view_node.has_method("load_encounter"):
		view_node.load_encounter(e)

func play_round(r: Dictionary, e: Dictionary) -> void:
	if view_node and view_node.has_method("play_round"):
		view_node.play_round(r, e)

func set_selection(ship: int, target: int) -> void:
	if view_node and view_node.has_method("set_selection"):
		view_node.set_selection(ship, target)

func show_plan(ship: Dictionary, forecast: Dictionary) -> void:
	if view_node and view_node.has_method("show_plan"):
		view_node.show_plan(ship, forecast)

func clear_plan() -> void:
	if view_node and view_node.has_method("clear_plan"):
		view_node.clear_plan()
