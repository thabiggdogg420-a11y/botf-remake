class_name BattleMarkers
extends Control
## Tactical markers over the 3D battle (port of STacticalViewport::PaintMarker), drawn from BattleCamera's last view:
## corner brackets around every hull in its owner's colour, fading as the ship grows on screen; a diamond on ships a
## few pixels across; rings on the selected ship (accent) and the target (alert); name and hull / shield bars on the
## selected ship, the target and the ship under the cursor. A radar in the lower right shows the battle from above,
## turned with the camera; clicking it flies the camera there, clicking a ship on it follows that ship.

const RADAR_RADIUS := 74.0
const RADAR_MARGIN := 14.0

var camera: BattleCamera
var show_radar := true
var _font: Font


func _ready() -> void:
	name = "BattleMarkers"
	set_anchors_preset(Control.PRESET_FULL_RECT)
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	_font = ThemeDB.fallback_font
	var radar := Radar.new()
	radar.markers = self
	add_child(radar)


static func owner_color(owner: int) -> Color:
	return Lcars.owner_color(owner)


static func selected_color() -> Color:
	return Color("#9fd8ff")


static func target_color() -> Color:
	return Lcars.ALERT


func _draw() -> void:
	if camera == null or camera.view == null:
		return
	var view := camera.view
	var t := Time.get_ticks_msec() / 1000.0
	for b in camera.bodies:
		var p := CameraRig.project(view, b.center, b.radius)
		if not p.visible:
			continue
		var c := Vector2(p.u * size.x, p.v * size.y)
		var r := p.radius * size.y
		var sel: bool = int(b.id) == camera.selected_id
		var tgt: bool = int(b.id) == camera.target_id
		var hover: bool = int(b.id) == camera.hover_id
		var key := sel or tgt or hover
		var base := target_color() if tgt else (selected_color() if sel else owner_color(int(b.owner)))
		# A hull filling much of the view (a follow shot) needs no frame: only its name and bars, kept compact.
		var large := r > size.y * 0.16
		if large and not key:
			continue
		var alpha := 0.95 if key else remap(clampf(r, 14.0, 110.0), 14.0, 110.0, 0.75, 0.16)
		# R3.5: at 24 v 24 a bracket on every hull drew a red fence over the enemy line; in a crowd only the selected,
		# targeted and hovered ships get brackets, the rest a faint pip.
		var crowded: bool = camera.bodies.size() > 12
		if crowded and not key:
			var pip := Color(base, 0.45)
			draw_circle(c, 2.2, pip)
			continue
		var col := Color(base.lightened(0.15 if key else 0.0), alpha)
		var h := minf(r * 0.55, size.y * 0.2) if large else maxf(r * 1.08, 9.0)
		var l := clampf(h * 0.32, 4.0, 22.0)
		var thick := 2.0 if key else 1.4
		if not large:
			for corner in 4:
				var sx := 1.0 if corner & 1 else -1.0
				var sy := 1.0 if corner & 2 else -1.0
				var q := c + Vector2(sx * h, sy * h)
				draw_polyline(PackedVector2Array([q - Vector2(sx * l, 0), q, q - Vector2(0, sy * l)]), col, thick, true)
		if r < 5.0:
			var d := 3.5
			draw_polyline(PackedVector2Array([c + Vector2(0, -d), c + Vector2(d, 0), c + Vector2(0, d), c + Vector2(-d, 0), c + Vector2(0, -d)]), col, 1.6, true)
		# Selection and target rings.
		var ring := maxf(h * 1.32, 14.0)
		if sel:
			draw_arc(c, ring + 1.5 * sin(t * 4.0), 0, TAU, 48, Color(selected_color(), 0.85), 1.6, true)
		if tgt:
			draw_arc(c, ring, 0, TAU, 48, Color(target_color(), 0.9), 1.6, true)
			for k in 4: # reticle ticks
				var dir := Vector2.from_angle(k * PI * 0.5 + PI * 0.25)
				draw_line(c + dir * (ring - 4.0), c + dir * (ring + 6.0), Color(target_color(), 0.9), 1.6, true)
		if not key:
			continue
		# Name above the bracket, bars below it.
		var name_text := str(b.get("name", ""))
		if name_text != "":
			var above := maxf(h, ring if (sel or tgt) else h) + 6.0
			draw_string(_font, c + Vector2(-60.0, -above), name_text.to_upper(), HORIZONTAL_ALIGNMENT_CENTER, 120, 12, col)
		var w := clampf(h * 2.0, 46.0, 180.0)
		var o := c + Vector2(-w * 0.5, h + 5.0)
		var hull := clampf(float(b.get("hull", 1.0)), 0.0, 1.0)
		var shields := clampf(float(b.get("shields", 0.0)), 0.0, 1.0)
		var hull_col := Color("#8fd18a") if hull > 0.5 else (Color("#ffcf6a") if hull > 0.25 else Lcars.ALERT)
		draw_rect(Rect2(o, Vector2(w, 3)), Color(0, 0, 0, 0.7))
		draw_rect(Rect2(o, Vector2(w * hull, 3)), hull_col)
		if shields > 0.0 or sel or tgt:
			draw_rect(Rect2(o + Vector2(0, 5), Vector2(w, 2)), Color(0, 0, 0, 0.7))
			draw_rect(Rect2(o + Vector2(0, 5), Vector2(w * shields, 2)), Lcars.SHIELD)


## World <-> radar: the battle seen from above, centred on its bounds and turned so the camera looks up the radar.
class Radar extends Control:
	var markers: BattleMarkers

	func _ready() -> void:
		name = "Radar"
		mouse_filter = Control.MOUSE_FILTER_STOP
		tooltip_text = "Radar: click to fly the camera there, click a ship to follow it"
		var d := BattleMarkers.RADAR_RADIUS * 2.0
		custom_minimum_size = Vector2(d, d)
		size = Vector2(d, d)
		set_anchors_preset(Control.PRESET_BOTTOM_RIGHT)
		position = Vector2.ZERO
		offset_left = -d - BattleMarkers.RADAR_MARGIN
		offset_top = -d - BattleMarkers.RADAR_MARGIN
		offset_right = -BattleMarkers.RADAR_MARGIN
		offset_bottom = -BattleMarkers.RADAR_MARGIN

	func _process(_d: float) -> void:
		var cam := markers.camera if markers != null else null
		visible = markers != null and markers.show_radar and cam != null and cam.view != null and not cam.bodies.is_empty()
		if visible:
			queue_redraw()

	func _axes() -> Array:
		var v := markers.camera.view
		var fwd := Vector3(v.forward.x, 0, v.forward.z)
		if fwd.length() < 0.2:
			fwd = Vector3(v.up.x, 0, v.up.z)
		fwd = fwd.normalized()
		return [Vector3(-fwd.z, 0, fwd.x), fwd] # right, ahead

	func _scale() -> float:
		return (BattleMarkers.RADAR_RADIUS - 8.0) / maxf(1.0, float(markers.camera.battle_bounds.radius) * 1.1)

	func to_radar(world: Vector3) -> Vector2:
		var ax := _axes()
		var p: Vector3 = world - markers.camera.battle_bounds.center
		return size * 0.5 + Vector2(p.dot(ax[0]), -p.dot(ax[1])) * _scale()

	func to_world(local: Vector2) -> Vector3:
		var ax := _axes()
		var q := (local - size * 0.5) / _scale()
		var w: Vector3 = markers.camera.battle_bounds.center + ax[0] * q.x - ax[1] * q.y
		w.y = markers.camera.free_focus.y if markers.camera.free_cam else markers.camera.battle_bounds.center.y
		return w

	func _draw() -> void:
		var cam := markers.camera
		var c := size * 0.5
		var rr := BattleMarkers.RADAR_RADIUS
		if cam == null or cam.view == null:
			return
		draw_circle(c, rr, Color(0.02, 0.03, 0.07, 0.72))
		draw_arc(c, rr, 0, TAU, 64, Color(0.55, 0.7, 1.0, 0.45), 1.2, true)
		draw_arc(c, rr * 0.5, 0, TAU, 48, Color(0.55, 0.7, 1.0, 0.15), 1.0, true)
		draw_line(c - Vector2(rr, 0), c + Vector2(rr, 0), Color(0.55, 0.7, 1.0, 0.12))
		draw_line(c - Vector2(0, rr), c + Vector2(0, rr), Color(0.55, 0.7, 1.0, 0.12))
		# The camera: its focus and a wedge toward it from where it stands.
		var eye := to_radar(cam.view.eye)
		var focus := to_radar(cam.smoother.current.focus)
		var span := (focus - eye).length()
		if span > 2.0:
			var half := atan(cam.view.half_tan_h())
			var dir := (focus - eye).normalized()
			var reach := minf(span * 1.4, rr * 2.0)
			var wedge := PackedVector2Array([eye, eye + dir.rotated(-half) * reach, eye + dir.rotated(half) * reach])
			var clipped := Geometry2D.intersect_polygons(wedge, _circle(c, rr - 1.0))
			for poly in clipped:
				draw_colored_polygon(poly, Color(0.55, 0.7, 1.0, 0.08))
		draw_arc(focus, 3.0, 0, TAU, 12, Color(1, 1, 1, 0.6), 1.0, true)
		for b in cam.bodies:
			var p := to_radar(b.center)
			if p.distance_to(c) > rr - 3.0:
				p = c + (p - c).normalized() * (rr - 3.0)
			var col := BattleMarkers.owner_color(int(b.owner))
			draw_circle(p, 3.2 if b.get("mine", false) else 2.8, col)
			if int(b.id) == cam.selected_id:
				draw_arc(p, 6.0, 0, TAU, 16, BattleMarkers.selected_color(), 1.4, true)
			if int(b.id) == cam.target_id:
				draw_arc(p, 6.0, 0, TAU, 16, BattleMarkers.target_color(), 1.4, true)

	static func _circle(c: Vector2, r: float) -> PackedVector2Array:
		var pts := PackedVector2Array()
		for k in 32:
			pts.append(c + Vector2.from_angle(TAU * k / 32.0) * r)
		return pts

	func _gui_input(event: InputEvent) -> void:
		var mb := event as InputEventMouseButton
		if mb == null or not mb.pressed or mb.button_index != MOUSE_BUTTON_LEFT:
			if event is InputEventMouseButton and (event as InputEventMouseButton).button_index in [MOUSE_BUTTON_WHEEL_UP, MOUSE_BUTTON_WHEEL_DOWN]:
				return # let the wheel through to the camera
			accept_event()
			return
		if mb.position.distance_to(size * 0.5) > BattleMarkers.RADAR_RADIUS:
			return
		accept_event()
		var cam := markers.camera
		var best := 0
		var best_d := 7.0
		for b in cam.bodies:
			var d := to_radar(b.center).distance_to(mb.position)
			if d < best_d:
				best_d = d
				best = int(b.id)
		if best != 0:
			cam.select_ship(best)
			cam.follow_fleet(best)
		else:
			cam.fly_to_point(to_world(mb.position))
