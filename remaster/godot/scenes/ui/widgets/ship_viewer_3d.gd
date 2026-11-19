class_name ShipViewer3D
extends SubViewportContainer
## F5 encyclopedia (20 Nov 2026): the entry's ship as a turning 3D model, the same hull the battle draws (BattleShip over
## BattleAssets: hero glb, or the procedural stand-in when a key has none). Turns slowly by itself; drag to turn it,
## wheel to zoom, double-click to reset. Own world, so the encyclopedia never touches a battle in progress.

var asset_key := ""
var owner_id := 0
var ship_name := ""
var auto_turn := true

var _vp: SubViewport
var _root: Node3D
var _pivot: Node3D
var _cam: Camera3D
var _ship: BattleShip
var _yaw := -35.0
var _pitch := -14.0
var _dist := 1.0
var _len := 4.0
var _drag := false
var _idle := 0.0


func _ready() -> void:
	stretch = true
	mouse_filter = Control.MOUSE_FILTER_STOP
	_vp = SubViewport.new()
	_vp.own_world_3d = true
	_vp.msaa_3d = Viewport.MSAA_4X
	_vp.transparent_bg = false
	add_child(_vp)
	_root = Node3D.new()
	_vp.add_child(_root)
	var we := WorldEnvironment.new()
	var env := Environment.new()
	env.background_mode = Environment.BG_COLOR
	env.background_color = Color(0.015, 0.02, 0.04)
	env.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	env.ambient_light_color = Color(0.2, 0.21, 0.26)
	env.tonemap_mode = Environment.TONE_MAPPER_ACES
	env.tonemap_exposure = 0.9
	env.glow_enabled = true
	env.glow_intensity = 0.3
	we.environment = env
	_root.add_child(we)
	var key := DirectionalLight3D.new()
	key.rotation_degrees = Vector3(-30, -50, 0)
	key.light_energy = CompatLook.sun(2.0)
	key.light_color = Color(1.0, 0.95, 0.88)
	_root.add_child(key)
	var rim := DirectionalLight3D.new()
	rim.rotation_degrees = Vector3(-10, 140, 0)
	rim.light_energy = CompatLook.sun(0.7)
	rim.light_color = Color(0.6, 0.72, 1.0)
	_root.add_child(rim)
	_pivot = Node3D.new()
	_root.add_child(_pivot)
	_ship = BattleShip.new()
	_pivot.add_child(_ship)
	_ship.setup({"id": 1, "owner": owner_id, "name": ship_name, "assetKey": asset_key, "maxHull": 100}, BattleAssets.shared())
	_len = maxf(_ship.length, 1.0)
	_dist = _len * 1.45
	_cam = Camera3D.new()
	_cam.fov = 32.0
	_cam.near = _len * 0.02
	_cam.far = _len * 40.0
	_root.add_child(_cam)
	_cam.current = true
	_place()


## The hull drawn (hero manifest key, "" for the procedural stand-in): tests and the frame proof read it.
func hero_key() -> String:
	return _ship.hero_key if _ship != null else ""


func _place() -> void:
	_pivot.rotation_degrees = Vector3(0, _yaw, 0)
	var p := deg_to_rad(_pitch)
	_cam.position = Vector3(0, -sin(p) * _dist, cos(p) * _dist)
	_cam.look_at(Vector3.ZERO, Vector3.UP)


func _process(delta: float) -> void:
	if _cam == null:
		return
	_idle += delta
	if auto_turn and not _drag and _idle > 2.0:
		_yaw += delta * 12.0
		_place()


func _gui_input(ev: InputEvent) -> void:
	if ev is InputEventMouseButton:
		if ev.button_index == MOUSE_BUTTON_LEFT:
			_drag = ev.pressed
			if ev.double_click:
				_yaw = -35.0
				_pitch = -14.0
				_dist = _len * 1.45
		elif ev.button_index == MOUSE_BUTTON_WHEEL_UP and ev.pressed:
			_dist = maxf(_dist * 0.9, _len * 0.8)
		elif ev.button_index == MOUSE_BUTTON_WHEEL_DOWN and ev.pressed:
			_dist = minf(_dist * 1.1, _len * 5.0)
		_idle = 0.0
		_place()
		accept_event()
	elif ev is InputEventMouseMotion and _drag:
		_yaw += ev.relative.x * 0.5
		_pitch = clampf(_pitch - ev.relative.y * 0.4, -80.0, 80.0)
		_idle = 0.0
		_place()
		accept_event()
