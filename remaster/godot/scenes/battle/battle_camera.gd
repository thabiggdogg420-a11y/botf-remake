class_name BattleCamera
extends Camera3D
## Tactical battle camera: a port of the Unreal ABotfCombatScene camera and STacticalViewport controls.
##
## Every mode proposes a target CameraRig.Rig; the player's orbit, zoom and pan sit on top of it; CameraRig.Smoother
## eases the shown rig there (log-space zoom, the short way round, an arc out and back in on long moves).
##   OVERVIEW   frames every ship; panning, cursor zoom or fly-to turn it into a free camera (Home resets it).
##   CHASE      the latest exchange of fire, both ships side-on.
##   SHIP       follows the selected ship from its rear quarter, sized to its hull.
##   TARGET     follows the target from its front quarter.
##   TOP-DOWN   straight down over the battle.
##   CINEMATIC  the BattleTimeline.Director shots: over the shoulder on shooters, reverse angles on impacts, an
##              establishing view of the formations between them.
## Mouse: wheel zooms toward the cursor, left-drag orbits, right-drag pans, middle-click resets. A click selects the ship
## under the cursor (ship_picked); a double-click follows it, or flies to that spot of empty space.
## Keys: C cinematic and back, F follow the selected ship, G follow the target, +/- zoom, Home frame the battle.
## Markers (BattleMarkers) bracket every ship in its owner's colour and ring the selection and the target.

enum Mode { OVERVIEW, CHASE, SHIP, TARGET, TOP_DOWN, CINEMATIC, ORBIT }
## ORBIT is not a mode of its own: set_mode(ORBIT) frees the camera where it stands (OVERVIEW, free).

const MODE_NAMES := ["OVERVIEW", "CHASE", "SHIP", "TARGET", "TOP-DOWN", "CINEMATIC"]
const MODE_HINTS := [
	"Tactical overview of the whole battle (Home)",
	"Follows the latest exchange of fire",
	"Follows the selected ship (F)",
	"Follows the target (G)",
	"Overhead view of the arena",
	"Follows the action of the round (C)",
]
const BASE_AZIMUTH := 90.0   ## The default side: the camera south of the arena (+Z), looking north.
const BASE_ELEVATION := 45.0
const DRAG_THRESHOLD := 4.0  ## Pixels a press may move and still be a click.

signal mode_changed(mode: int)
signal ship_picked(ship_id: int)

@export var mode := Mode.CINEMATIC
@export var show_markers := true

## Selection, set by the host (set_selection) or by clicking ships.
var selected_id := 0
var target_id := 0
## The player's orbit and zoom on top of the mode's framing (degrees / factor).
var orbit_yaw_offset := 0.0
var orbit_pitch_offset := 0.0
var zoom_factor := 1.0
## Free camera: set by panning, cursor zoom or fly-to in OVERVIEW; Home returns to framing the whole battle.
var free_cam := false
var free_focus := Vector3.ZERO
var free_distance := 200.0
var max_distance := 400.0
var transition := 0.0        ## Seconds of soft focus easing left after a mode or subject change.
var smoother := CameraRig.Smoother.new()
var view: CameraRig.View     ## The view last filmed: markers and picking use it.
## Last frame's ships: {id, center, radius, owner, mine, heading, name, hull, shields}.
var bodies: Array = []
var battle_bounds := {"center": Vector3.ZERO, "radius": 100.0}
var hover_id := 0
var markers: BattleMarkers

## Set by BattleScene each frame: the player's and the enemy's centroids, and the player's formation size.
var has_sides := false
var side_ours := Vector3.ZERO
var side_theirs := Vector3.ZERO
var side_ours_spread := 0.0
## Legacy (the old free orbit drifted when idle); ignored.
var auto_orbit_speed := 0.0

var _prev_mode := Mode.OVERVIEW
var _prev_free := false
var _shot_key := ""
var _shot_seed := 0
var _shot_start := 0.0
var _cut := false
var _snap_next := false
var _drag_button := 0
var _drag_moved := false
var _press_pos := Vector2.ZERO
var _last_pos := Vector2.ZERO
var _double := false
var _time := 0.0
var _radius_cache := {}
var _clear_subject := 0
var _clear_angle := Vector3(0, 0, 1)
var _clear_check := 0.0

## Legacy orbit API (battle_demo, tools): the free camera's focus, distance and angles in the old convention.
var orbit_focus: Vector3:
	get: return free_focus
	set(v):
		free_focus = v
		_free_here()
var orbit_distance: float:
	get: return free_distance
	set(v):
		free_distance = v
		_free_here()
var orbit_yaw: float: ## Old convention: the camera at Basis.from_euler(pitch, yaw) * +Z from the focus.
	get: return 90.0 - (BASE_AZIMUTH + orbit_yaw_offset)
	set(v):
		orbit_yaw_offset = CameraRig.angle_delta(BASE_AZIMUTH, 90.0 - v)
		_free_here()
var orbit_pitch: float:
	get: return -(BASE_ELEVATION + orbit_pitch_offset)
	set(v):
		orbit_pitch_offset = -v - BASE_ELEVATION
		_free_here()


func _ready() -> void:
	fov = 48.0
	near = 0.3
	far = 6000.0
	current = true
	var layer := CanvasLayer.new()
	layer.name = "Markers"
	add_child(layer)
	markers = BattleMarkers.new()
	markers.camera = self
	layer.add_child(markers)


func _free_here() -> void:
	if mode != Mode.OVERVIEW:
		mode = Mode.OVERVIEW
		mode_changed.emit(mode)
	free_cam = true
	_snap_next = true


# ------------------------------------------------------------------------------------------------------------------
# Modes and selection
# ------------------------------------------------------------------------------------------------------------------

func set_mode(m: int) -> void:
	if m == Mode.ORBIT:
		detach_to_free_cam()
		return
	m = clampi(m, 0, Mode.CINEMATIC)
	if m == mode and not free_cam and orbit_yaw_offset == 0.0 and orbit_pitch_offset == 0.0 and zoom_factor == 1.0:
		return
	# A mode starts from its own framing; the camera then eases there.
	var changed := m != mode
	mode = m
	orbit_yaw_offset = 0.0
	orbit_pitch_offset = 0.0
	zoom_factor = 1.0
	free_cam = false
	transition = 1.0
	_clear_subject = 0
	if changed:
		mode_changed.emit(mode)


func mode_name(m := -1) -> String:
	return MODE_NAMES[mode if m < 0 else m]


## C: to cinematic, and back to the mode (and free camera) it left.
func toggle_cinematic() -> void:
	if mode == Mode.CINEMATIC:
		set_mode(_prev_mode)
		if _prev_mode == Mode.OVERVIEW and _prev_free:
			free_cam = true
	else:
		_prev_mode = mode
		_prev_free = free_cam
		set_mode(Mode.CINEMATIC)


## Orbit, zoom and pan back to the mode's own framing; in OVERVIEW that frames the whole battle.
func reset_view() -> void:
	orbit_yaw_offset = 0.0
	orbit_pitch_offset = 0.0
	zoom_factor = 1.0
	free_cam = false
	transition = 1.0


## Home: frame the whole battle.
func frame_battle() -> void:
	set_mode(Mode.OVERVIEW)
	reset_view()


func set_selection(ship: int, target: int) -> void:
	var subject_changed := (mode == Mode.SHIP and ship != selected_id) or (mode == Mode.TARGET and target != target_id)
	selected_id = ship
	target_id = target
	if subject_changed:
		transition = 1.0


## Keep exactly what is on screen, now as a free OVERVIEW camera the player steers.
func detach_to_free_cam() -> void:
	if free_cam and mode == Mode.OVERVIEW:
		return
	var was := mode
	mode = Mode.OVERVIEW
	free_cam = true
	zoom_factor = 1.0
	if smoother.primed:
		var now := smoother.current
		free_focus = now.focus
		free_distance = now.distance
		orbit_yaw_offset = CameraRig.angle_delta(BASE_AZIMUTH, now.azimuth)
		orbit_pitch_offset = now.elevation - BASE_ELEVATION
	if was != mode:
		mode_changed.emit(mode)


func body(fleet_id: int) -> Dictionary:
	if fleet_id == 0:
		return {}
	for b in bodies:
		if int(b.id) == fleet_id:
			return b
	return {}


## Fly to and follow a ship: own ship in SHIP mode, an enemy in TARGET mode.
func follow_fleet(fleet_id: int) -> void:
	var b := body(fleet_id)
	if b.is_empty():
		return
	if b.get("mine", false):
		selected_id = fleet_id
		set_mode(Mode.SHIP)
	else:
		target_id = fleet_id
		set_mode(Mode.TARGET)
	transition = 1.0


## The ship under a screen point (0..1 across and down the image); 0 = none.
func pick_at(screen01: Vector2) -> int:
	if view == null:
		return 0
	var shrunk := []
	for b in bodies:
		shrunk.append({"id": b.id, "center": b.center, "radius": b.radius * 0.8}) # spheres overstate flat hulls
	return CameraRig.pick(view, screen01.x, screen01.y, shrunk)


## A click on a ship: select it here (own ship / target) and tell the host.
func select_ship(fleet_id: int) -> void:
	var b := body(fleet_id)
	if b.is_empty():
		return
	if b.get("mine", false):
		selected_id = fleet_id
	else:
		target_id = fleet_id
	# Deferred: the host may rebuild its layout (and re-parent this view) in response, not mid-input.
	ship_picked.emit.call_deferred(fleet_id)


## Wheel: in OVERVIEW zoom toward the point under the cursor, which stays under it; elsewhere about the subject.
func zoom_at(notches: float, screen01: Vector2) -> void:
	if not (mode in [Mode.OVERVIEW, Mode.TOP_DOWN]) or view == null or not smoother.primed:
		zoom_factor = clampf(CameraRig.zoom_factor_after(zoom_factor, notches), 0.04, 8.0)
		return
	detach_to_free_cam()
	var new_distance := clampf(free_distance * pow(0.85, notches), _min_free_distance(), max_distance)
	var ground: Variant = CameraRig.ray_plane(view, screen01.x, screen01.y, free_focus.y)
	if ground is Vector3:
		free_focus += (ground - free_focus) * (1.0 - new_distance / maxf(CameraRig.MIN_DISTANCE, free_distance))
	free_distance = new_distance


## Drag distance as a fraction of the image; frees the camera. The battle moves with the cursor.
func pan(drag01: Vector2) -> void:
	if view == null:
		return
	detach_to_free_cam()
	# Ground covered by the image at the focus; a low camera sees farther up the screen, so vertical drags go further.
	var width := 2.0 * free_distance * view.half_tan_h()
	var height := width / view.aspect / maxf(0.25, sin(deg_to_rad(clampf(BASE_ELEVATION + orbit_pitch_offset, 5.0, 88.0))))
	var right := Vector3(view.right.x, 0, view.right.z).normalized()
	var ahead := Vector3(view.forward.x, 0, view.forward.z)
	if ahead.length() < 0.5:
		ahead = Vector3(view.up.x, 0, view.up.z)
	ahead = ahead.normalized()
	free_focus += -right * (drag01.x * width) + ahead * (drag01.y * height)


func add_orbit(delta_yaw: float, delta_pitch: float) -> void:
	orbit_yaw_offset = fmod(orbit_yaw_offset + delta_yaw, 360.0)
	# The elevation (mode base + pitch) is clamped by the rig; keep the offset itself within a sane range.
	orbit_pitch_offset = clampf(orbit_pitch_offset + delta_pitch, -150.0, 90.0)


## Fly the free camera to the spot of the battle plane under a screen point.
func fly_to(screen01: Vector2) -> void:
	if view == null:
		return
	detach_to_free_cam()
	var ground: Variant = CameraRig.ray_plane(view, screen01.x, screen01.y, free_focus.y)
	if ground is Vector3:
		fly_to_point(ground)


func fly_to_point(world: Vector3) -> void:
	detach_to_free_cam()
	free_focus = world
	free_distance = clampf(free_distance * 0.55, _min_free_distance() * 2.0, max_distance)
	transition = 1.0


func _min_free_distance() -> float:
	return BattleScene.CELL * 0.18


# ------------------------------------------------------------------------------------------------------------------
# Input (STacticalViewport)
# ------------------------------------------------------------------------------------------------------------------

func _unhandled_input(event: InputEvent) -> void:
	if handle_input(event):
		get_viewport().set_input_as_handled()


func _screen01(pos: Vector2) -> Vector2:
	var size := get_viewport().get_visible_rect().size
	return Vector2(pos.x / maxf(1.0, size.x), pos.y / maxf(1.0, size.y))


## Returns true when the event was used.
func handle_input(event: InputEvent) -> bool:
	if event is InputEventKey:
		return _handle_key(event)
	if event is InputEventMouseButton:
		var mb := event as InputEventMouseButton
		match mb.button_index:
			MOUSE_BUTTON_WHEEL_UP, MOUSE_BUTTON_WHEEL_DOWN:
				if mb.pressed:
					var n := mb.factor if mb.factor > 0.0 else 1.0
					zoom_at(n if mb.button_index == MOUSE_BUTTON_WHEEL_UP else -n, _screen01(mb.position))
				return true
			MOUSE_BUTTON_MIDDLE:
				if mb.pressed:
					reset_view()
				return true
			MOUSE_BUTTON_LEFT, MOUSE_BUTTON_RIGHT:
				if mb.pressed:
					_drag_button = mb.button_index
					_drag_moved = false
					_press_pos = mb.position
					_last_pos = mb.position
					_double = false
					if mb.double_click and mb.button_index == MOUSE_BUTTON_LEFT:
						_double = true
						var at := _screen01(mb.position)
						var fleet := pick_at(at)
						if fleet != 0:
							select_ship(fleet)
							follow_fleet(fleet)
						else:
							fly_to(at)
				elif mb.button_index == _drag_button:
					# A left press that never became a drag is a click: select the ship under the cursor.
					if _drag_button == MOUSE_BUTTON_LEFT and not _drag_moved and not _double:
						var fleet := pick_at(_screen01(mb.position))
						if fleet != 0:
							select_ship(fleet)
					_drag_button = 0
					_drag_moved = false
				return true
	if event is InputEventMouseMotion:
		var mm := event as InputEventMouseMotion
		if _drag_button == 0:
			hover_id = pick_at(_screen01(mm.position))
			return false
		if mm.button_mask == 0:
			_drag_button = 0 # released where we did not hear it
			return false
		if not _drag_moved and mm.position.distance_to(_press_pos) < DRAG_THRESHOLD:
			return true # still a click
		_drag_moved = true
		var d := mm.position - _last_pos
		_last_pos = mm.position
		if _drag_button == MOUSE_BUTTON_RIGHT:
			var size := get_viewport().get_visible_rect().size
			pan(Vector2(d.x / maxf(1.0, size.x), d.y / maxf(1.0, size.y)))
		else:
			add_orbit(d.x * 0.35, -d.y * 0.35)
		return true
	return false


func _handle_key(k: InputEventKey) -> bool:
	if not k.pressed or k.echo or k.is_command_or_control_pressed() or k.alt_pressed:
		return false
	match k.keycode:
		KEY_C:
			toggle_cinematic()
		KEY_F:
			if selected_id != 0:
				follow_fleet(selected_id)
		KEY_G:
			if target_id != 0:
				follow_fleet(target_id)
		KEY_HOME:
			frame_battle()
		KEY_EQUAL, KEY_PLUS, KEY_KP_ADD:
			zoom_at(1.0, Vector2(0.5, 0.5))
		KEY_MINUS, KEY_KP_SUBTRACT:
			zoom_at(-1.0, Vector2(0.5, 0.5))
		_:
			return false
	return true


# ------------------------------------------------------------------------------------------------------------------
# Filming
# ------------------------------------------------------------------------------------------------------------------

## Called by BattleScene each frame.
##   bodies:  the visible ships, {id, center, radius, owner, mine, heading (degrees, CameraRig azimuth of the bow),
##            name, hull, shields}.
##   chase:   [from, to] fleet ids of the latest exchange of fire, or [] for none.
##   shot:    the cinematic director's shot: {key, kind (BattleTimeline.Job, -1 = wide), subject, subject_size,
##            other} (positions in world units), plus centroid and spread of the battle.
func film(delta: float, time: float, p_bodies: Array, chase: Array, shot: Dictionary) -> void:
	_time = time
	bodies = p_bodies
	# Without a host choosing them (the demo, tools), F and G still have a ship to follow.
	if selected_id == 0 or target_id == 0:
		for b in bodies:
			if b.get("mine", false) and selected_id == 0:
				selected_id = int(b.id)
			elif not b.get("mine", false) and target_id == 0:
				target_id = int(b.id)
	var aspect := 16.0 / 9.0
	var size := get_viewport().get_visible_rect().size if is_inside_tree() else Vector2.ZERO
	if size.y > 0.0:
		aspect = size.x / size.y
	var all := CameraRig.bounds_of(bodies)
	if bodies.is_empty():
		all = {"center": shot.get("centroid", Vector3.ZERO), "radius": BattleScene.CELL * 4.0}
	all.radius = maxf(all.radius, BattleScene.CELL * 1.0)
	battle_bounds = all
	var frame_all := CameraRig.frame_distance(all.radius, fov, aspect, 1.05)
	max_distance = maxf(BattleScene.CELL * 24.0, frame_all * 3.0)
	var shown_azimuth := smoother.current.azimuth if smoother.primed else BASE_AZIMUTH

	# Each mode proposes a rig: what to look at, from which side, and how far away to fit it.
	var target := CameraRig.Rig.new(all.center, frame_all, BASE_AZIMUTH, BASE_ELEVATION)
	var subject_radius := 0.0 # > 0 when the camera films one hull: it may come close, never inside
	match mode:
		Mode.SHIP, Mode.TARGET:
			# Follow one ship, sized to its hull: own ship from the rear quarter, the target from the front quarter.
			var b := body(selected_id if mode == Mode.SHIP else target_id)
			if not b.is_empty():
				target.focus = b.center
				target.azimuth = float(b.get("heading", 0.0)) + (215.0 if mode == Mode.SHIP else 35.0)
				target.elevation = 18.0
				target.distance = CameraRig.frame_distance(b.radius, fov, aspect, 1.7)
				subject_radius = b.radius
				# Ships fly in formation at true scale: a cruiser beside a scout can fill the shot. Swing round to a
				# clear angle, chosen when the subject changes and kept until it is blocked again.
				_clear_check -= delta
				if int(b.id) != _clear_subject or (_clear_check <= 0.0 and _blocked(target.copy(), _clear_angle, b)):
					_clear_subject = int(b.id)
					_clear_angle = _clear_view(target, b)
					_clear_check = 1.0
				target.azimuth += _clear_angle.x
				target.elevation += _clear_angle.y
				target.distance *= _clear_angle.z
		Mode.CHASE:
			# The latest exchange of fire: both ships in frame, seen side-on to the line between them.
			var a := body(chase[0]) if chase.size() == 2 else {}
			var b := body(chase[1]) if chase.size() == 2 else {}
			if not a.is_empty() and not b.is_empty():
				var line: Vector3 = b.center - a.center
				target.focus = (a.center + b.center) * 0.5
				var line_az := rad_to_deg(atan2(line.z, line.x))
				# The side of the line nearer the current view, so the camera never whips across.
				var left := line_az + 90.0
				var right := line_az - 90.0
				target.azimuth = left if absf(CameraRig.angle_delta(shown_azimuth, left)) < absf(CameraRig.angle_delta(shown_azimuth, right)) else right
				target.elevation = 24.0
				target.distance = CameraRig.frame_distance(line.length() * 0.5 + maxf(a.radius, b.radius), fov, aspect, 1.1)
		Mode.CINEMATIC:
			var pose := _cinematic_shot(delta, shot, time)
			target = CameraRig.rig_from_eye(pose[0], pose[1])
		Mode.TOP_DOWN:
			target.elevation = CameraRig.MAX_ELEVATION
		_:
			if free_cam:
				target.focus = free_focus
				target.distance = free_distance

	# The player's orbit and zoom on top of the mode's framing.
	target.azimuth += orbit_yaw_offset
	var wanted := target.elevation + orbit_pitch_offset
	target.elevation = clampf(wanted, CameraRig.MIN_ELEVATION, CameraRig.MAX_ELEVATION)
	orbit_pitch_offset += target.elevation - wanted # no dead zone past the limits
	if not (mode == Mode.OVERVIEW and free_cam):
		target.distance *= zoom_factor
		var min_distance := subject_radius * 1.3 if subject_radius > 0.0 else _min_free_distance()
		target.distance = clampf(target.distance, min_distance, max_distance)
	else:
		free_distance = clampf(free_distance, _min_free_distance(), max_distance)

	# Ease there. A followed ship keeps moving, so the focus tracks it tightly once the camera has arrived; right after
	# a change of mode or subject the focus eases softly, so the move reads as a flight. A director cut is a cut.
	transition = maxf(0.0, transition - delta)
	if _snap_next or (_cut and mode == Mode.CINEMATIC and transition <= 0.0):
		smoother.snap(target)
	_snap_next = false
	_cut = false
	var half_life := 0.45 if mode == Mode.CINEMATIC else (0.35 if mode == Mode.CHASE else 0.22)
	var tracking := subject_radius > 0.0 or mode == Mode.CHASE or mode == Mode.CINEMATIC
	var focus_half_life := lerpf(0.05, half_life, clampf(transition, 0.0, 1.0)) if tracking else half_life
	var rig_shown := smoother.step(target, delta, half_life, focus_half_life)
	view = CameraRig.view_of(rig_shown, fov, aspect)
	global_transform = Transform3D(view.basis(), view.eye)
	if markers != null:
		markers.visible = show_markers
		markers.queue_redraw()


## True when another hull stands between the camera of rig (turned by angle: azimuth, elevation offsets) and the subject.
## angle = (azimuth offset, elevation offset, distance factor). Bounding spheres overstate long, flat hulls, so an
## occluder counts from 0.6 of its radius; the stretch of the line inside the subject's own sphere does not count.
func _blocked(rig: CameraRig.Rig, angle: Vector3, subject: Dictionary) -> bool:
	rig.azimuth += angle.x
	rig.elevation += angle.y
	rig.distance *= angle.z
	var eye := rig.focus + CameraRig.offset_of(rig.azimuth, rig.elevation) * rig.distance
	var seg := rig.focus - eye
	var length := maxf(1e-3, seg.length())
	var k_max := clampf(1.0 - float(subject.radius) / length, 0.0, 1.0)
	for o in bodies:
		if int(o.id) == int(subject.id):
			continue
		var k := clampf((o.center - eye).dot(seg) / (length * length), 0.0, k_max)
		if (eye + seg * k).distance_to(o.center) < float(o.radius) * 0.6:
			return true
	return false


## The first clear angle around the mode's own, nearest first; failing that (a scout tucked in beside a cruiser), pull
## back from above until the neighbours no longer fill the shot.
func _clear_view(rig: CameraRig.Rig, subject: Dictionary) -> Vector3:
	for d in [1.0, 2.0, 3.5]:
		for a in [Vector2.ZERO, Vector2(35, 0), Vector2(-35, 0), Vector2(0, 22), Vector2(70, 10), Vector2(-70, 10),
				Vector2(35, 30), Vector2(-35, 30), Vector2(120, 15), Vector2(-120, 15), Vector2(0, 50), Vector2(180, 20)]:
			var angle := Vector3(a.x, a.y, d)
			if not _blocked(rig.copy(), angle, subject):
				return angle
	return Vector3(0, 50, 3.5)


## Bounding-sphere radius of a ship's hull as built, measured once from its meshes (the shield bubble excluded), so
## framing and picking fit whatever model and scale the ship has. Falls back to BattleShip.radius().
func hull_radius(ship: BattleShip) -> float:
	var key := ship.get_instance_id()
	if _radius_cache.has(key):
		return _radius_cache[key]
	var r := ship.radius()
	if ship.model != null:
		var inv := ship.global_transform.affine_inverse()
		var box := AABB()
		var any := false
		for n in ship.model.find_children("*", "VisualInstance3D", true, false):
			var gi := n as VisualInstance3D
			if gi == ship.shield or not (gi is GeometryInstance3D) or gi is GPUParticles3D or gi is CPUParticles3D:
				continue
			var local: AABB = (inv * gi.global_transform) * gi.get_aabb()
			box = local if not any else box.merge(local)
			any = true
		if any and box.size.length() > 1e-3:
			r = box.size.length() * 0.5 + box.get_center().length() * 0.5
	if _radius_cache.size() > 256:
		_radius_cache.clear()
	_radius_cache[key] = r
	return r


## The rig the current mode asks for this frame, before easing (tests, tools).
func target_rig() -> CameraRig.Rig:
	if mode == Mode.OVERVIEW and free_cam:
		var o := CameraRig.Rig.new(free_focus, free_distance, BASE_AZIMUTH + orbit_yaw_offset, BASE_ELEVATION + orbit_pitch_offset)
		o.elevation = clampf(o.elevation, CameraRig.MIN_ELEVATION, CameraRig.MAX_ELEVATION)
		return o
	return smoother.current.copy()


## Jump to the target rig on the next frame instead of easing (screenshots, tests).
func snap() -> void:
	_snap_next = true


## Pushes the eye back along the view line until it is outside every hull's shield shell (1.6 radii). A cut to a
## close shot of a small ship used to put the lens inside a neighbour's shield: the 8 Nov close strip opened white.
func _clear_of_hulls(eye: Vector3, look: Vector3) -> Vector3:
	var back := (eye - look).normalized()
	if back.length() < 0.5:
		return eye
	for pass_i in 4:
		var moved := false
		for b in bodies:
			var c: Vector3 = b.center
			var r: float = float(b.radius) * 2.4
			if eye.distance_to(c) < r:
				eye += back * (r - eye.distance_to(c) + 2.0)
				moved = true
		if not moved:
			break
	return eye


## The cinematic director's framing: [eye, look]. shot.kind < 0 is a wide shot of the battle.
func _cinematic_shot(_delta: float, shot: Dictionary, time: float) -> Array:
	var key: String = shot.get("key", "")
	var kind: int = shot.get("kind", -1)
	var centroid: Vector3 = shot.get("centroid", Vector3.ZERO)
	var subject: Vector3 = shot.get("subject", centroid)
	var subject_size: float = shot.get("subject_size", 10.0)
	var other: Vector3 = shot.get("other", centroid)
	var spread: float = shot.get("spread", 60.0)
	if key != _shot_key:
		_shot_key = key
		_shot_seed = hash(key)
		_shot_start = time
		_cut = true
	var rng := RandomNumberGenerator.new()
	rng.seed = _shot_seed
	var side_sign := 1.0 if rng.randf() < 0.5 else -1.0
	var lift := rng.randf_range(0.15, 0.45)
	var dist_k := rng.randf_range(0.85, 1.25)
	var drift := (time - _shot_start) * 0.05
	var eye: Vector3
	var look: Vector3
	var aspect := 16.0 / 9.0
	var vsize := get_viewport().get_visible_rect().size if is_inside_tree() else Vector2.ZERO
	if vsize.y > 0.0:
		aspect = vsize.x / vsize.y
	# R3.5 framing (coordinator review of the 8 Nov strips: hulls cropped at the edges, specks in a void): every shot
	# frames whole hulls. Two-ship shots keep shooter and target both inside the frame with margin; wide shots hold
	# both fleets side-on from a raised three-quarter angle; no shot puts the lens inside a hull or its shield.
	if kind < 0 and has_sides:
		var to_enemy := side_theirs - side_ours
		to_enemy.y = 0.0
		var fwd := to_enemy.normalized() if to_enemy.length() > 1e-3 else Vector3(1, 0, 0)
		var side := fwd.cross(Vector3.UP).normalized() * side_sign
		var mid := (side_ours + side_theirs) * 0.5
		var r := to_enemy.length() * 0.5 + side_ours_spread * 0.5 + 20.0
		var dist := CameraRig.frame_distance(r, fov, aspect, 1.0) * 0.9
		var elev := deg_to_rad(28.0 + lift * 20.0)
		# Side-on to the line between the fleets, a little behind ours so the player's ships sit nearer the lens.
		var dir := (side * cos(elev) - fwd * 0.35 + Vector3.UP * sin(elev)).normalized()
		eye = mid + dir.rotated(Vector3.UP, drift * 0.3) * dist
		look = mid.lerp(side_ours, 0.12)
	elif kind < 0:
		var r := clampf(spread * 0.6, 35.0, 400.0)
		var a := time * 0.04 + float(_shot_seed % 628) / 100.0
		var dist := CameraRig.frame_distance(r, fov, aspect, 1.0)
		eye = centroid + (Vector3(sin(a), 0.0, cos(a)) * cos(0.55) + Vector3.UP * sin(0.55)) * dist
		look = centroid
	else:
		var other_size: float = shot.get("other_size", subject_size)
		var line := other - subject
		var flat := Vector3(line.x, 0.0, line.z)
		if flat.length() < 1e-3:
			flat = Vector3(1, 0, 0)
		var fwd := flat.normalized()
		var side := fwd.cross(Vector3.UP).normalized() * side_sign
		var two := kind in [BattleTimeline.Job.BEAM, BattleTimeline.Job.PULSE, BattleTimeline.Job.TORPEDO, BattleTimeline.Job.RAM,
			BattleTimeline.Job.SHIELD, BattleTimeline.Job.HULL, BattleTimeline.Job.INTERCEPT]
		var shooter_kind := kind in [BattleTimeline.Job.BEAM, BattleTimeline.Job.PULSE, BattleTimeline.Job.TORPEDO, BattleTimeline.Job.RAM]
		var far := line.length() > 7.0 * maxf(subject_size, other_size)
		if two and far and shooter_kind:
			# Long range: over the shooter's shoulder. The shooter is whole and large in frame, the launch leaves its
			# tubes toward a target small but visible beyond it (the director cuts to the target for the impact).
			# Sized so the shooter's whole hull sits in the lower third of the frame with margin, the line of fire
			# running away from it toward the target.
			var dist := CameraRig.frame_distance(subject_size * 1.5, fov, aspect, 1.0) * dist_k
			var dir := (-fwd + side * 0.9 + Vector3.UP * (0.45 + lift * 0.4)).normalized()
			eye = subject + dir.rotated(Vector3.UP, drift * 0.5) * dist
			look = subject + (other - subject).normalized() * subject_size * 1.2
		elif two and far:
			# Long range impact: the struck ship whole, seen from the attacker's side so the shot arrives toward us.
			var dist := maxf(subject_size * 2.8, 14.0) * dist_k
			var dir := (fwd + side * 0.8 + Vector3.UP * (0.3 + lift * 0.5)).normalized()
			eye = subject + dir.rotated(Vector3.UP, drift * 0.5) * dist
			look = subject
		elif two:
			# Shooter and target together, seen across the line of fire: both hulls whole, the shot crossing the frame.
			var shooter_shot := kind in [BattleTimeline.Job.BEAM, BattleTimeline.Job.PULSE, BattleTimeline.Job.TORPEDO, BattleTimeline.Job.RAM]
			var r := line.length() * 0.5 + maxf(subject_size, other_size) * 0.75
			var dist := CameraRig.frame_distance(r, fov, aspect, 1.15) * dist_k
			var elev := deg_to_rad(16.0 + lift * 30.0)
			var lean := -0.35 if shooter_shot else 0.35 # the camera leans toward the shooter (or the struck ship)
			var dir := (side * cos(elev) + fwd * lean + Vector3.UP * sin(elev)).normalized()
			look = subject.lerp(other, 0.5)
			eye = look + dir.rotated(Vector3.UP, drift * 0.4) * dist
		else:
			# One ship (an explosion, a cloak): framed whole at about three hull lengths.
			var k := 1.8 if kind == BattleTimeline.Job.EXPLOSION else 1.0
			var dist := maxf(subject_size * 2.8, 12.0) * dist_k * k
			var dir := (side + fwd * 0.6 + Vector3.UP * (0.25 + lift)).normalized()
			eye = subject + dir.rotated(Vector3.UP, drift) * dist
			look = subject
	eye = _clear_of_hulls(eye, look)
	return [eye, look]
