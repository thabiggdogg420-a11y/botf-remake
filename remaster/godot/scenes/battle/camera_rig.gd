class_name CameraRig
extends RefCounted
## Engine-independent maths of the tactical battle camera: a GDScript port of
## remaster/game/Source/BOTFRemake/Private/BotfCameraRig.h (orbit rig, framing, projection, picking, easing).
##
## The battle is filmed through one orbit rig: a focus point, a distance, an azimuth around the vertical and an
## elevation above the battle plane. Each camera mode proposes a target rig; the player's orbit, zoom and pan adjust
## it; Smoother eases the shown rig toward the target, lifting the camera on long moves so a fly-to arcs out and back
## in rather than sliding through hulls. The same View projects hulls for the tactical markers and turns a click into
## a picked ship.
##
## Frame: Godot world units, Y up. Azimuth is measured around +Y from +X toward +Z: the camera sits at
## focus + (cos el cos az, sin el, cos el sin az) * distance. Differences from the C++: the field of view is vertical
## (Camera3D.fov with KEEP_HEIGHT), and the floors on radius and distance are in Godot units.
## Unit-tested in scenes/battle/tests/test_camera_rig.gd.

const MIN_ELEVATION := -80.0 ## The camera may look up at the battle from below.
const MAX_ELEVATION := 88.0  ## Straight down would lose the screen's up direction.
const MIN_DISTANCE := 0.05


class Rig:
	var focus := Vector3.ZERO
	var distance := 200.0
	var azimuth := 90.0
	var elevation := 45.0

	func _init(p_focus := Vector3.ZERO, p_distance := 200.0, p_azimuth := 90.0, p_elevation := 45.0) -> void:
		focus = p_focus
		distance = p_distance
		azimuth = p_azimuth
		elevation = p_elevation

	func copy() -> Rig:
		return Rig.new(focus, distance, azimuth, elevation)


## A camera: eye, unit axes (right = forward x up, up = right x forward) and the lens. fov is the vertical field of
## view in degrees; aspect = width / height of the rendered image.
class View:
	var eye := Vector3.ZERO
	var forward := Vector3(0, 0, -1)
	var right := Vector3(1, 0, 0)
	var up := Vector3(0, 1, 0)
	var fov := 48.0
	var aspect := 16.0 / 9.0

	func half_tan_v() -> float:
		return tan(deg_to_rad(fov) * 0.5)

	func half_tan_h() -> float:
		return half_tan_v() * aspect

	## The Camera3D basis for this view (Godot cameras look down -Z).
	func basis() -> Basis:
		return Basis(right, up, -forward)


class ScreenPoint:
	var visible := false
	var u := 0.0      ## 0 left, 1 right
	var v := 0.0      ## 0 top, 1 bottom
	var radius := 0.0 ## projected radius as a fraction of the image height
	var depth := 0.0


## Signed difference b - a folded into (-180, 180].
static func angle_delta(a: float, b: float) -> float:
	var d := fmod(b - a, 360.0)
	if d > 180.0:
		d -= 360.0
	if d <= -180.0:
		d += 360.0
	return d


static func offset_of(azimuth: float, elevation: float) -> Vector3:
	var el := deg_to_rad(clampf(elevation, MIN_ELEVATION, MAX_ELEVATION))
	var az := deg_to_rad(azimuth)
	return Vector3(cos(el) * cos(az), sin(el), cos(el) * sin(az))


static func view_of(rig: Rig, fov: float, aspect: float) -> View:
	var offset := offset_of(rig.azimuth, rig.elevation)
	var v := View.new()
	v.eye = rig.focus + offset * maxf(MIN_DISTANCE, rig.distance)
	v.forward = -offset
	v.right = v.forward.cross(Vector3.UP).normalized()
	v.up = v.right.cross(v.forward)
	v.fov = fov
	v.aspect = maxf(0.1, aspect)
	return v


## The rig that looks from eye at look (no roll).
static func rig_from_eye(eye: Vector3, look: Vector3) -> Rig:
	var off := eye - look
	var d := off.length()
	if d < 1e-6:
		return Rig.new(look, MIN_DISTANCE, 90.0, 45.0)
	return Rig.new(look, d, rad_to_deg(atan2(off.z, off.x)), rad_to_deg(asin(clampf(off.y / d, -1.0, 1.0))))


## Distance at which a sphere fills the narrower half of the frame, divided by margin (1.15: a little space around it).
static func frame_distance(radius: float, fov: float, aspect: float, margin := 1.15) -> float:
	var half_v := deg_to_rad(fov) * 0.5
	var half_h := atan(tan(half_v) * maxf(0.1, aspect))
	return maxf(MIN_DISTANCE, radius) * margin / sin(minf(half_h, half_v))


## Screen position of a point; radius = the sphere's projected radius as a fraction of the image height. Points behind
## the camera are not visible.
static func project(view: View, point: Vector3, world_radius := 0.0) -> ScreenPoint:
	var p := ScreenPoint.new()
	var d := point - view.eye
	var depth := d.dot(view.forward)
	if depth <= 1e-3:
		return p
	p.depth = depth
	p.u = 0.5 + 0.5 * d.dot(view.right) / (depth * view.half_tan_h())
	p.v = 0.5 - 0.5 * d.dot(view.up) / (depth * view.half_tan_v())
	p.radius = world_radius / (2.0 * depth * view.half_tan_v())
	p.visible = p.u > -0.1 and p.u < 1.1 and p.v > -0.1 and p.v < 1.1
	return p


## Unit ray from the eye through a screen point (u, v as in ScreenPoint).
static func ray_through(view: View, u: float, v: float) -> Vector3:
	var sx := (2.0 * u - 1.0) * view.half_tan_h()
	var sy := (1.0 - 2.0 * v) * view.half_tan_v()
	return (view.forward + view.right * sx + view.up * sy).normalized()


## Where the ray through (u, v) meets the horizontal plane y = height; null when it runs parallel or away.
static func ray_plane(view: View, u: float, v: float, height: float) -> Variant:
	var dir := ray_through(view, u, v)
	if absf(dir.y) < 1e-6:
		return null
	var t := (height - view.eye.y) / dir.y
	if t <= 0.0:
		return null
	return view.eye + dir * t


## The ship under the cursor. bodies: Array of {id: int, center: Vector3, radius: float}. The nearest hull whose
## sphere the ray enters; failing that, the hull whose projected outline lies within slack (a fraction of the image
## height, ~18 px at 1080p) of the cursor, so a speck-sized escort seen from across the battle can still be clicked.
## 0 = nothing.
static func pick(view: View, u: float, v: float, bodies: Array, slack := 0.017) -> int:
	var dir := ray_through(view, u, v)
	var best := 0
	var best_t := INF
	for b in bodies:
		var oc: Vector3 = view.eye - b.center
		var half := oc.dot(dir)
		var c: float = oc.dot(oc) - b.radius * b.radius
		var disc := half * half - c
		if disc < 0.0:
			continue
		var t := -half - sqrt(disc)
		var hit := t if t > 0.0 else (0.0 if -half + sqrt(disc) > 0.0 else -1.0) # inside the sphere counts
		if hit >= 0.0 and hit < best_t:
			best_t = hit
			best = int(b.id)
	if best != 0:
		return best
	var best_gap := slack
	for b in bodies:
		var p := project(view, b.center, b.radius)
		if p.depth <= 0.0:
			continue
		var du := (p.u - u) * view.aspect # in image-height units
		var dv := p.v - v
		var gap := sqrt(du * du + dv * dv) - p.radius
		if gap < best_gap:
			best_gap = gap
			best = int(b.id)
	return best


## A sphere around every body: the centre of their bounding box and the farthest hull edge from it.
## Returns {center: Vector3, radius: float}.
static func bounds_of(bodies: Array) -> Dictionary:
	if bodies.is_empty():
		return {"center": Vector3.ZERO, "radius": 0.0}
	var lo: Vector3 = bodies[0].center
	var hi := lo
	for b in bodies:
		var r := Vector3.ONE * float(b.radius)
		lo = lo.min(b.center - r)
		hi = hi.max(b.center + r)
	var center := (lo + hi) * 0.5
	var radius := 0.0
	for b in bodies:
		radius = maxf(radius, (b.center - center).length() + float(b.radius))
	return {"center": center, "radius": radius}


## Exponential easing toward a target: after half_life seconds half the remaining gap is left.
static func approach(current: float, target: float, dt: float, half_life: float) -> float:
	if dt <= 0.0:
		return current
	if half_life <= 0.0:
		return target
	return target + (current - target) * pow(2.0, -dt / half_life)


static func approach_angle(current: float, target: float, dt: float, half_life: float) -> float:
	var gap := angle_delta(current, target)
	return current + gap - approach(gap, 0.0, dt, half_life) # approach() leaves the part of the gap still to close


## Wheel zoom: each notch toward the screen (positive) brings the camera 15% closer.
static func zoom_factor_after(factor: float, notches: float) -> float:
	return factor * pow(0.85, notches)


## Eases the shown rig toward the target rig. Distance eases in log space (zooming feels even from a hull close-up to
## the whole battle); angles take the short way round. While the focus still has far to travel the camera stands off
## by part of that gap, so a jump across the battle arcs out and back in.
class Smoother:
	var current := Rig.new()
	var primed := false
	var travel_lift := 0.35

	## focus_half_life < 0: the focus eases like everything else. A shorter one keeps a moving subject centred.
	func step(target: Rig, dt: float, half_life: float, focus_half_life := -1.0) -> Rig:
		var fh := half_life if focus_half_life < 0.0 else focus_half_life
		if not primed:
			current = target.copy()
			primed = true
		else:
			current.focus = Vector3(CameraRig.approach(current.focus.x, target.focus.x, dt, fh),
				CameraRig.approach(current.focus.y, target.focus.y, dt, fh),
				CameraRig.approach(current.focus.z, target.focus.z, dt, fh))
			current.distance = exp(CameraRig.approach(log(maxf(CameraRig.MIN_DISTANCE, current.distance)),
				log(maxf(CameraRig.MIN_DISTANCE, target.distance)), dt, half_life))
			current.azimuth = CameraRig.approach_angle(current.azimuth, target.azimuth, dt, half_life)
			current.elevation = CameraRig.approach(current.elevation, target.elevation, dt, half_life)
		var shown := current.copy()
		shown.distance += travel_lift * (target.focus - current.focus).length()
		return shown

	func snap(target: Rig) -> void:
		current = target.copy()
		primed = true
