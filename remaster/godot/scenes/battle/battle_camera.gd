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
var _crop_check := 0.0
var _crop_subject := 0
var _crop_adj := Vector3(0, 0, 1)
var _cut_pose := []          ## The accepted cutaway [eye, look]; empty on the tactical view.
var _tactical := []          ## The tactical [eye, look] for this round.
var _tactical_t := -1.0
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
## The backdrop planet (set by BattleScene) and the vista shot that frames the fleet with it, at true scale.
var planet_center := Vector3.ZERO
var planet_radius := 0.0
var force_vista := false       ## Tools/tests: hold the planet vista.
var vista_active := false      ## True while the filmed tactical shot is the planet vista.

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
	far = 9000.0 # the battle planet sits ~4300 units back (BattleEnvironment)
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
				# Same rule as the director's shots (coordinator, 8 Nov): no other hull over 10% of the frame cut by
				# the edge. Checked when the subject changes and once a second; the first clean angle is kept.
				if _crop_check <= 0.0 or int(b.id) != _crop_subject:
					_crop_check = 1.0
					_crop_subject = int(b.id)
					_crop_adj = Vector3(0, 0, 1)
					var base := target.copy()
					for az in [0.0, 40.0, -40.0, 80.0, -80.0, 130.0, -130.0, 180.0]:
						var found := false
						for dk in [1.0, 1.3, 1.7]:
							var r := base.copy()
							r.azimuth += az
							r.distance *= dk
							var v := CameraRig.view_of(r, fov, aspect)
							var g := [b]
							if _group_ok(v.eye, r.focus, g, aspect) or (az == 180.0 and dk == 1.7):
								_crop_adj = Vector3(az, 0, dk)
								found = true
								break
						if found:
							break
				_crop_check -= delta
				target.azimuth += _crop_adj.x
				target.distance *= _crop_adj.z
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


## Screen fit of a bounding sphere seen from eye toward look: [inside (with margin), height fraction of the frame].
func _sphere_fit(eye: Vector3, look: Vector3, c: Vector3, r: float, aspect: float, margin := 0.9) -> Array:
	var b := BattleCamera.safe_look(look - eye)
	var v := b.inverse() * (c - eye)
	var z := -v.z
	if z <= r * 1.05:
		return [false, 1.0]
	var th := tan(deg_to_rad(fov) * 0.5)
	var y := v.y / (z * th)
	var x := v.x / (z * th * aspect)
	var ry := r / (z * th)
	var rx := ry / aspect
	return [absf(x) + rx <= margin and absf(y) + ry <= margin, ry]


## R3.5 pass 3 (coordinator, 8 Nov): the default is the TACTICAL view. Every live ship fitted from a raised three-quarter
## angle with about 10% margin, recomputed each round and eased. A station far bigger than the ships is left out of the
## fit (it may sit part-framed) so the ships stay readable; the distance is capped so a ship never falls below a
## minimum size on screen.
func _tactical_pose(aspect: float, side_sign: float) -> Array:
	var live := []
	var radii := []
	for b in bodies:
		if float(b.radius) > 0.0:
			live.append(b)
			radii.append(float(b.radius))
	if live.is_empty():
		return [Vector3(0, 200, 300), Vector3.ZERO]
	radii.sort()
	var median: float = radii[radii.size() / 2]
	var fit := live.duplicate()
	var th0 := tan(deg_to_rad(fov) * 0.5)
	var tmp := _fit_distance(fit, aspect, side_sign)
	if median / (tmp[1] * th0) < 0.022:
		# A station far bigger than the ships would leave them specks: fit the ships and let the station sit part-framed.
		fit = []
		for b in live:
			if float(b.radius) <= median * 3.0 or live.size() <= 2:
				fit.append(b)
	var res := _fit_distance(fit, aspect, side_sign)
	return [res[0] + res[2] * res[1], res[0]]


## [mid, distance, dir]: the nearest raised three-quarter view holding every body in fit whole with ~10% margin.
func _fit_distance(fit: Array, aspect: float, side_sign: float) -> Array:
	var lo := Vector3(INF, INF, INF)
	var hi := -lo
	for b in fit:
		var c: Vector3 = b.center
		lo = lo.min(c)
		hi = hi.max(c)
	var mid := (lo + hi) * 0.5
	# Look across the line between the fleets, from the player's side and above.
	var ours := Vector3.ZERO
	var theirs := Vector3.ZERO
	var no := 0
	var nt := 0
	for b in fit:
		if b.get("mine", false):
			ours += b.center
			no += 1
		else:
			theirs += b.center
			nt += 1
	var fwd := Vector3(1, 0, 0)
	if no > 0 and nt > 0:
		var d: Vector3 = theirs / nt - ours / no
		d.y = 0.0
		if d.length() > 1e-3:
			fwd = d.normalized()
	var side := fwd.cross(Vector3.UP).normalized() * side_sign
	# Raised three-quarter view (about 40 degrees), drifting slowly round the battle.
	var dir := (side * 0.85 - fwd * 0.4 + Vector3.UP * 0.8).normalized().rotated(Vector3.UP, _time * 0.008)
	# Fit the screen-space box of every hull to ~85% of the frame and centre it: little empty frame, no crop.
	var th := tan(deg_to_rad(fov) * 0.5)
	var dist := 0.0
	for b in fit:
		dist = maxf(dist, mid.distance_to(b.center) + float(b.radius))
	dist = maxf(dist * 2.0, 20.0)
	for it in 10:
		var eye := mid + dir * dist
		var bs := BattleCamera.safe_look(mid - eye)
		var inv := bs.inverse()
		var x0 := INF
		var x1 := -INF
		var y0 := INF
		var y1 := -INF
		var near_z := INF
		for b in fit:
			var v: Vector3 = inv * ((b.center as Vector3) - eye)
			var z := maxf(-v.z, 1e-3)
			near_z = minf(near_z, z - float(b.radius))
			var ry := float(b.radius) / (z * th)
			var x := v.x / (z * th * aspect)
			var y := v.y / (z * th)
			x0 = minf(x0, x - ry / aspect)
			x1 = maxf(x1, x + ry / aspect)
			y0 = minf(y0, y - ry)
			y1 = maxf(y1, y + ry)
		var cx := (x0 + x1) * 0.5
		var cy := (y0 + y1) * 0.5
		var fill := maxf((x1 - x0) * 0.5, (y1 - y0) * 0.5)
		mid += bs.x * cx * dist * th * aspect * 0.8 + bs.y * cy * dist * th * 0.8
		var k := clampf(fill / 0.86, 0.6, 1.6)
		if near_z < dist * 0.25:
			k = maxf(k, 1.0) # never fly into the nearest hull to fill the frame
		dist *= k
	return [mid, dist, dir]


## A cutaway candidate is accepted only if shooter and target are both whole in frame and neither fills more than 40%
## of the frame height (and no other hull sits between the lens and them).
func _shot_ok(eye: Vector3, look: Vector3, a: Vector3, ar: float, b: Vector3, br: float, aspect: float) -> bool:
	for s in [[a, ar], [b, br]]:
		var f: Array = _sphere_fit(eye, look, s[0], s[1], aspect, 0.92)
		if not f[0] or f[1] > 0.40:
			return false
	# Any other hull is either whole in frame or wholly out of it (a big one crossing the edge is a crop), never
	# around the lens, and never a near hull filling more than 40% of the frame.
	for o in bodies:
		var c: Vector3 = o.center
		var orad: float = float(o.radius)
		if c.distance_to(a) < 0.1 or c.distance_to(b) < 0.1 or orad <= 0.0:
			continue
		if eye.distance_to(c) < orad * 1.6:
			return false
		var f2: Array = _sphere_fit(eye, look, c, orad, aspect, 1.0)
		if f2[1] > 0.40:
			return false
		var is_station := int(o.get("id", 0)) >= 2000000 # a station may sit part-framed (coordinator, 8 Nov)
		if not is_station and not f2[0] and f2[1] > 0.10 and not _sphere_outside(eye, look, c, orad, aspect):
			return false
	return true


## The cinematic director's framing: [eye, look]. Tactical by default; a cutaway only when the scene offers one
## (about 1 round in 4, or a kill) and one of the candidate angles passes _shot_ok. A bad shot is rejected, never patched.
func _cinematic_shot(_delta: float, shot: Dictionary, time: float) -> Array:
	var key: String = shot.get("key", "")
	var kind: int = shot.get("kind", -1)
	var aspect := 16.0 / 9.0
	var vsize := get_viewport().get_visible_rect().size if is_inside_tree() else Vector2.ZERO
	if vsize.y > 0.0:
		aspect = vsize.x / vsize.y
	if shot.has("film"):
		return _film_pose(shot, key, time)
	if key != _shot_key:
		var was_tactical := _shot_key.begins_with("wide") or _shot_key == "" or _cut_pose.is_empty()
		_shot_key = key
		_shot_seed = hash(key)
		_shot_start = time
		_cut_pose = []
		if kind >= 0:
			_cut_pose = _group_shot(shot.get("subject", Vector3.ZERO), shot.get("other", shot.get("subject", Vector3.ZERO)), aspect)
		if not _cut_pose.is_empty() or not was_tactical:
			_cut = true
	# Tactical uses a narrower lens (R3.5: ships fill more of the frame, less perspective shrink at the far side).
	fov = 48.0 if not _cut_pose.is_empty() else 36.0
	if not _cut_pose.is_empty():
		var drift := (time - _shot_start) * 0.03
		var e0: Vector3 = _cut_pose[0]
		var l0: Vector3 = _cut_pose[1]
		return [l0 + (e0 - l0).rotated(Vector3.UP, drift), l0]
	if _tactical.is_empty() or time - _tactical_t > 1.0 or time < _tactical_t:
		_tactical_t = time
		_tactical = _tactical_pose(aspect, 1.0)
	vista_active = planet_radius > 0.0 and (force_vista or bool(shot.get("vista", false)))
	if vista_active:
		var vp := vista_pose(_tactical)
		fov = 48.0
		return vp
	return _tactical


## Battlechoreo film cuts (BattleScene._film_shot): [eye, look] recomputed every frame so the lens rides with the
## moving ships. A new key is a hard cut. behind: over the shooter's shoulder toward its target; torpedo: chasing the
## salvo onto the target; flyby: a fixed lens ahead of the escort's path, panning as it passes.
var _film_eye := Vector3.ZERO

func _film_pose(shot: Dictionary, key: String, time: float) -> Array:
	var a: Vector3 = shot.subject
	var b: Vector3 = shot.get("other", a)
	var ra := maxf(float(shot.get("subject_size", 10.0)) * 0.5, 3.0)
	var rb := maxf(float(shot.get("other_size", 10.0)) * 0.5, 3.0)
	var to_b := b - a
	if to_b.length() < 1e-3:
		to_b = shot.get("subject_fwd", Vector3(1, 0, 0))
	var dir := to_b.normalized()
	var side := dir.cross(Vector3.UP).normalized()
	if side.length() < 0.5:
		side = Vector3(0, 0, 1)
	var sgn := 1.0 if (hash(key) & 1) == 0 else -1.0
	var fresh := key != _shot_key
	if fresh:
		_shot_key = key
		_shot_start = time
		_cut = true
		_cut_pose = []
	var eye: Vector3
	var look: Vector3
	match str(shot.film):
		"torpedo":
			var p: Vector3 = shot.get("torp", a)
			var d := (b - p)
			var dn := d.normalized() if d.length() > 1e-3 else dir
			var s2 := dn.cross(Vector3.UP).normalized()
			eye = p - dn * maxf(rb * 1.6, 14.0) + s2 * sgn * maxf(rb * 0.7, 6.0) + Vector3.UP * maxf(rb * 0.5, 4.0)
			look = p.lerp(b, 0.7)
			fov = 50.0
		"flyby":
			if fresh or _film_eye == Vector3.ZERO:
				var fwd: Vector3 = shot.get("subject_fwd", dir)
				_film_eye = a + fwd * ra * 9.0 + fwd.cross(Vector3.UP).normalized() * sgn * ra * 3.5 + Vector3.UP * ra * 1.2
			eye = _film_eye
			look = a.lerp(b, 0.15)
			fov = 42.0
		_:
			eye = a - dir * ra * 3.4 + side * sgn * ra * 1.3 + Vector3.UP * ra * 1.0
			look = a.lerp(b, 0.65)
			fov = 46.0
	eye = _clear_of_hulls(eye, look)
	return [eye, look]


## The planet vista: from a little beside and above the fleet, looking back past it so the fleet sits low in the
## foreground and the planet limb fills the far side of the frame. `tactical` is the tactical [eye, look].
func vista_pose(tactical: Array) -> Array:
	var mid: Vector3 = tactical[1]
	var dist := maxf(((tactical[0] as Vector3) - mid).length(), 40.0)
	var to_p := planet_center - mid
	var dp := to_p.normalized()
	var side := dp.cross(Vector3.UP).normalized()
	var eye := mid - dp * dist * 1.1 + side * dist * 0.7 + Vector3.UP * dist * 0.12
	var a := (mid - eye).normalized()
	var b := (planet_center - eye).normalized()
	var look_dir := (a * 0.8 + b).normalized()
	return [eye, eye + look_dir * dist]


func _sphere_outside(eye: Vector3, look: Vector3, c: Vector3, r: float, aspect: float) -> bool:
	var bs := BattleCamera.safe_look(look - eye)
	var v := bs.inverse() * (c - eye)
	var z := -v.z
	if z < -r:
		return true
	if z <= 0.0:
		return false
	var th := tan(deg_to_rad(fov) * 0.5)
	var ry := r / (z * th)
	return absf(v.x / (z * th * aspect)) - ry / aspect > 1.0 or absf(v.y / (z * th)) - ry > 1.0


func _big_enough(eye: Vector3, look: Vector3, a: Vector3, ar: float, b: Vector3, br: float, aspect: float) -> bool:
	return maxf(_sphere_fit(eye, look, a, ar, aspect)[1], _sphere_fit(eye, look, b, br, aspect)[1]) >= 0.06


## R3.5 playback (coordinator, 8 Nov): a group shot of the 2-6 ships engaged right now: the director's pair plus the
## nearest ships around them. Accepted only when every group hull is whole, the group's median hull is at least 8%
## of frame height, the group fills the frame (<= ~15% empty along its long axis), no hull is over 40%, and no other
## hull above 10% of frame height is cut by the edge. Falls back to smaller groups; [] when nothing qualifies.
func _group_shot(a: Vector3, b: Vector3, aspect: float) -> Array:
	group_stats["shots"] = int(group_stats.get("shots", 0)) + 1
	var near := []
	for o in bodies:
		if float(o.radius) > 0.0:
			near.append(o)
	if near.is_empty():
		return []
	var mid0 := a.lerp(b, 0.5)
	var reach := maxf(a.distance_to(b) * 0.6, 1.0)
	near.sort_custom(func(x, y): return minf((x.center as Vector3).distance_to(a), (x.center as Vector3).distance_to(b)) < minf((y.center as Vector3).distance_to(a), (y.center as Vector3).distance_to(b)))
	var key := []
	for o in near:
		var c: Vector3 = o.center
		if c.distance_to(a) < 0.5 or c.distance_to(b) < 0.5:
			key.append(o)
	var extra := []
	for o in near:
		if key.has(o):
			continue
		if minf((o.center as Vector3).distance_to(a), (o.center as Vector3).distance_to(b)) < reach:
			extra.append(o)
	var line := b - a
	var flat := Vector3(line.x, 0.0, line.z)
	var fwd := flat.normalized() if flat.length() > 1e-3 else Vector3(1, 0, 0)
	for n_extra in [mini(4, extra.size()), mini(2, extra.size()), 0]:
		var group: Array = key + extra.slice(0, n_extra)
		if group.is_empty():
			continue
		for elev in [25.0, 40.0, 12.0, 55.0, 70.0]:
			for az in [90.0, -90.0, 135.0, -135.0, 45.0, -45.0, 180.0, 0.0]:
				var e := deg_to_rad(elev)
				var flat_dir: Vector3 = fwd.rotated(Vector3.UP, deg_to_rad(az))
				var dir: Vector3 = (flat_dir * cos(e) + Vector3.UP * sin(e)).normalized()
				# M1 director profile (24v24: 825 of 1,365 rejections): in a crowd a neighbour crosses the frame edge.
				# Take that neighbour into the group and refit, up to 8 hulls, instead of dropping the angle.
				var g: Array = group.duplicate()
				var fit: Array = _fit_dir(g, dir, aspect, 0.9)
				var eye: Vector3 = fit[0] + dir * float(fit[1])
				var ok := _group_ok(eye, fit[0], g, aspect)
				while not ok and _offender != null and g.size() < 8 and not g.has(_offender):
					g.append(_offender)
					fit = _fit_dir(g, dir, aspect, 0.9)
					eye = fit[0] + dir * float(fit[1])
					ok = _group_ok(eye, fit[0], g, aspect)
				if ok:
					group_stats["accepted"] = int(group_stats.get("accepted", 0)) + 1
					return [eye, fit[0]]
	group_stats["no shot"] = int(group_stats.get("no shot", 0)) + 1
	return []


## [look, distance]: the group's screen box fitted to `fill` of the frame along its long axis, centred.
func _fit_dir(group: Array, dir: Vector3, aspect: float, fill_to: float) -> Array:
	var th := tan(deg_to_rad(fov) * 0.5)
	var mid := Vector3.ZERO
	for o in group:
		mid += o.center
	mid /= float(group.size())
	var dist := 0.0
	for o in group:
		dist = maxf(dist, mid.distance_to(o.center) + float(o.radius))
	dist = maxf(dist * 2.5, 6.0)
	var rmax := 0.0
	for o in group:
		rmax = maxf(rmax, float(o.radius))
	for it in 12:
		var eye := mid + dir * dist
		var bs := BattleCamera.safe_look(mid - eye)
		var inv := bs.inverse()
		var x0 := INF
		var x1 := -INF
		var y0 := INF
		var y1 := -INF
		for o in group:
			var v: Vector3 = inv * ((o.center as Vector3) - eye)
			var z := maxf(-v.z, 1e-3)
			var ry := float(o.radius) / (z * th)
			x0 = minf(x0, v.x / (z * th * aspect) - ry / aspect)
			x1 = maxf(x1, v.x / (z * th * aspect) + ry / aspect)
			y0 = minf(y0, v.y / (z * th) - ry)
			y1 = maxf(y1, v.y / (z * th) + ry)
		mid += bs.x * (x0 + x1) * 0.5 * dist * th * aspect * 0.8 + bs.y * (y0 + y1) * 0.5 * dist * th * 0.8
		var k := maxf((x1 - x0) * 0.5, (y1 - y0) * 0.5) / fill_to
		dist *= clampf(k, 0.6, 1.6) if is_finite(k) else 1.0
		# M1 director profile: "group hull over 40%" rejected 425 of 617 candidates (6v6) because a tight pair fitted
		# to 90% of the frame blows each hull past the 40% cap. Never come closer than the biggest hull at ~34%.
		dist = maxf(dist, rmax / (0.34 * th))
	return [mid, dist]


## Basis.looking_at that never errors: a zero or non-finite direction keeps a level forward view, and a direction
## parallel to UP picks another up vector (Odo, M0 verify: "The target vector can't be zero" from _group_shot).
static func safe_look(fwd: Vector3) -> Basis:
	if not fwd.is_finite() or fwd.length_squared() < 1e-10:
		return Basis.looking_at(Vector3.FORWARD, Vector3.UP)
	var up := Vector3.UP if absf(fwd.normalized().dot(Vector3.UP)) < 0.999 else Vector3.BACK
	return Basis.looking_at(fwd, up)


## Why candidates fail (director profiling, M1): reason -> count, plus "accepted" and "shots" (group shots asked for).
var group_stats := {}

func _reject(reason: String) -> bool:
	group_stats[reason] = int(group_stats.get(reason, 0)) + 1
	return false


var _offender = null ## The hull that last failed a group shot by crossing the frame edge.

func _group_ok(eye: Vector3, look: Vector3, group: Array, aspect: float) -> bool:
	_offender = null
	var sizes := []
	for o in group:
		var f: Array = _sphere_fit(eye, look, o.center, float(o.radius), aspect, 0.97)
		if not f[0]:
			return _reject("group hull cut by edge")
		if f[1] > 0.40:
			return _reject("group hull over 40%")
		sizes.append(f[1])
	sizes.sort()
	if sizes[sizes.size() / 2] * 2.0 < 0.08: # median hull height (diameter) at least 8% of the frame
		return _reject("median hull under 8%")
	for o in bodies:
		if group.has(o) or float(o.radius) <= 0.0:
			continue
		var c: Vector3 = o.center
		if eye.distance_to(c) < float(o.radius) * 1.6:
			return _reject("lens inside another hull")
		var f2: Array = _sphere_fit(eye, look, c, float(o.radius), aspect, 1.0)
		if f2[1] > 0.40 and eye.distance_to(c) < eye.distance_to(look):
			return _reject("near hull over 40%")
		if not f2[0] and f2[1] > 0.10 and not _sphere_outside(eye, look, c, float(o.radius), aspect):
			_offender = o
			return _reject("other hull cut by edge")
	return true


## The hull radius of the body nearest p (the director passes hull lengths; the camera fits bounding spheres).
func _radius_near(p: Vector3, fallback: float) -> float:
	var best := INF
	var r := fallback
	for b in bodies:
		var d: float = (b.center as Vector3).distance_to(p)
		if d < best:
			best = d
			r = float(b.radius)
	return r if best < 1.0 else fallback
