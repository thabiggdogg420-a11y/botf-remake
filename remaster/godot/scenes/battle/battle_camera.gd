class_name BattleCamera
extends Camera3D
## Two modes: CINEMATIC follows BattleTimeline.Director shots (over-the-shoulder on shooters, reverse angles on
## impacts and explosions, a slow establishing orbit while ships manoeuvre), ORBIT is a free camera
## (drag = rotate, wheel = zoom, right-drag = pan). Press C to toggle; any drag switches to ORBIT.

enum Mode { CINEMATIC, ORBIT }

signal mode_changed(mode: int)

@export var mode := Mode.CINEMATIC
@export var orbit_distance := 120.0
@export var orbit_yaw := -35.0
@export var orbit_pitch := -28.0
@export var orbit_focus := Vector3.ZERO
@export var auto_orbit_speed := 3.0 ## Degrees per second while ORBIT is idle.

var _desired_pos := Vector3.ZERO
var _desired_look := Vector3.ZERO
var _look := Vector3.ZERO
var _shot_key := ""
var _shot_seed := 0
var _shot_start := 0.0
var _idle := 0.0
var _dragging := 0
var _snap := true
## Set by BattleScene each frame: the player's and the enemy's centroids, and the player's formation size.
var has_sides := false
var side_ours := Vector3.ZERO
var side_theirs := Vector3.ZERO
var side_ours_spread := 0.0


func _ready() -> void:
	fov = 48.0
	near = 0.3
	far = 6000.0
	current = true


func set_mode(m: int) -> void:
	if m == mode:
		return
	mode = m
	if mode == Mode.ORBIT:
		# Start the orbit from where the camera is.
		var off := global_position - orbit_focus
		orbit_distance = clampf(off.length(), 15.0, 600.0)
		orbit_yaw = rad_to_deg(atan2(off.x, off.z))
		orbit_pitch = -rad_to_deg(asin(clampf(off.y / maxf(off.length(), 1e-3), -1.0, 1.0)))
	_snap = mode == Mode.CINEMATIC
	mode_changed.emit(mode)


func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventKey and event.pressed and not event.echo and event.keycode == KEY_C:
		set_mode(Mode.ORBIT if mode == Mode.CINEMATIC else Mode.CINEMATIC)
	elif event is InputEventMouseButton:
		if event.button_index == MOUSE_BUTTON_WHEEL_UP and event.pressed:
			set_mode(Mode.ORBIT)
			orbit_distance = maxf(12.0, orbit_distance * 0.9)
		elif event.button_index == MOUSE_BUTTON_WHEEL_DOWN and event.pressed:
			set_mode(Mode.ORBIT)
			orbit_distance = minf(800.0, orbit_distance * 1.1)
		elif event.button_index in [MOUSE_BUTTON_LEFT, MOUSE_BUTTON_RIGHT]:
			_dragging = event.button_index if event.pressed else 0
	elif event is InputEventMouseMotion and _dragging != 0:
		set_mode(Mode.ORBIT)
		_idle = 0.0
		if _dragging == MOUSE_BUTTON_LEFT:
			orbit_yaw -= event.relative.x * 0.3
			orbit_pitch = clampf(orbit_pitch - event.relative.y * 0.3, -85.0, 85.0)
		else:
			var right := global_basis.x
			var fwd := Vector3(global_basis.z.x, 0, global_basis.z.z).normalized()
			orbit_focus += (-right * event.relative.x + fwd * -event.relative.y) * orbit_distance * 0.0015


## Called by BattleScene each frame. subject / other: world positions (other may equal subject); sizes in units.
## shot_key changes when the director cuts; kind is the BattleTimeline.Job that motivated the shot (-1 = wide).
func frame(delta: float, shot_key: String, kind: int, subject: Vector3, subject_size: float, other: Vector3,
		centroid: Vector3, spread: float, time: float) -> void:
	if mode == Mode.ORBIT:
		_idle += delta
		if _idle > 3.0 and _dragging == 0:
			orbit_yaw += auto_orbit_speed * delta
		var dir := Basis.from_euler(Vector3(deg_to_rad(orbit_pitch), deg_to_rad(orbit_yaw), 0)) * Vector3(0, 0, 1)
		global_position = orbit_focus + dir * orbit_distance
		look_at(orbit_focus, Vector3.UP)
		return
	if shot_key != _shot_key:
		_shot_key = shot_key
		_shot_seed = hash(shot_key)
		_shot_start = time
		_snap = true
	var rng := RandomNumberGenerator.new()
	rng.seed = _shot_seed
	var side_sign := 1.0 if rng.randf() < 0.5 else -1.0
	var lift := rng.randf_range(0.15, 0.45)
	var dist_k := rng.randf_range(0.85, 1.25)
	var drift := (time - _shot_start) * 0.05
	if kind < 0 and has_sides and side_ours.distance_to(side_theirs) > 40.0:
		# Fleets apart (the opening rounds): behind and above the player's formation, looking at the enemy, so our
		# ships read as ships rather than specks at the arena edge.
		var to_enemy := side_theirs - side_ours
		to_enemy.y = 0.0
		var fwd := to_enemy.normalized() if to_enemy.length() > 1e-3 else Vector3(1, 0, 0)
		var side := fwd.cross(Vector3.UP).normalized() * side_sign
		var r := clampf(side_ours_spread * 0.9 + 28.0, 30.0, 85.0)
		var base := -fwd * r + side * r * 0.5 + Vector3.UP * r * (0.3 + lift * 0.4)
		_desired_pos = side_ours + base.rotated(Vector3.UP, drift * 0.5)
		_desired_look = side_ours.lerp(side_theirs, 0.35)
	elif kind < 0:
		# Establishing shot: slow orbit around the battle, high enough to read the formations.
		var r := clampf(spread * 0.6, 35.0, 90.0)
		var a := time * 0.04 + float(_shot_seed % 628) / 100.0
		_desired_pos = centroid + Vector3(sin(a), 0.0, cos(a)) * r + Vector3.UP * r * 0.3
		_desired_look = centroid
	else:
		var to_other := other - subject
		to_other.y = 0.0
		if to_other.length() < 1e-3:
			to_other = Vector3(1, 0, 0)
		var fwd := to_other.normalized()
		var side := fwd.cross(Vector3.UP).normalized() * side_sign
		var dist := maxf(subject_size * 2.6, 10.0) * dist_k
		var shooter_shot := kind in [BattleTimeline.Job.BEAM, BattleTimeline.Job.PULSE, BattleTimeline.Job.TORPEDO, BattleTimeline.Job.RAM]
		if shooter_shot:
			# Over the shoulder: behind and beside the shooter, looking down the line of fire.
			var base := -fwd * dist + side * dist * 0.55 + Vector3.UP * dist * lift
			_desired_pos = subject + base.rotated(Vector3.UP, drift)
			_desired_look = subject.lerp(other, 0.45)
		else:
			# Reverse angle on the struck ship, from the attacker's side.
			var k := 1.6 if kind == BattleTimeline.Job.EXPLOSION else 1.0
			var base := fwd * dist * k + side * dist * 0.7 * k + Vector3.UP * dist * lift * k
			_desired_pos = subject + base.rotated(Vector3.UP, drift)
			_desired_look = subject
	if _snap:
		global_position = _desired_pos
		_look = _desired_look
		_snap = false
	else:
		var kp := 1.0 - exp(-delta * 1.8)
		var kl := 1.0 - exp(-delta * 3.5)
		global_position = global_position.lerp(_desired_pos, kp)
		_look = _look.lerp(_desired_look, kl)
	if global_position.distance_to(_look) > 0.01:
		look_at(_look, Vector3.UP)
