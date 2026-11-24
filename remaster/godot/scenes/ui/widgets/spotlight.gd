class_name Spotlight
extends Control
## The tutorial's spotlight (ONBOARDING-AND-MOBILE-PLAN.md 1.2): dims everything on screen except one control and pulses a ring
## round it. Clicks pass straight through (mouse_filter ignore), so the player can press the lit control, and the screen is
## never trapped behind the dim. point_at() takes a rect in global (canvas) coordinates; an empty rect hides it.
## Reusable: the mobile layout's first-run beats use the same node.

const DIM := Color(0.01, 0.02, 0.05, 0.62)
const MARGIN := 10.0

var hole := Rect2()
var color := Color("#ffcc66")
var _t := 0.0

func _init() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	set_anchors_preset(Control.PRESET_FULL_RECT)
	visible = false

## The control to light, in global coordinates; Rect2() turns the spotlight off.
func point_at(r: Rect2) -> void:
	hole = r
	visible = r.size.x > 0.0 and r.size.y > 0.0
	queue_redraw()

func _process(delta: float) -> void:
	if visible:
		_t += delta
		queue_redraw()

func _draw() -> void:
	if not visible:
		return
	var origin := get_global_rect().position
	var h := Rect2(hole.position - origin, hole.size).grow(MARGIN)
	var full := Rect2(Vector2.ZERO, size)
	h = h.intersection(full) if h.intersects(full) else h
	# four bands around the hole: everything else is dimmed
	draw_rect(Rect2(0, 0, full.size.x, maxf(0.0, h.position.y)), DIM)
	draw_rect(Rect2(0, h.end.y, full.size.x, maxf(0.0, full.size.y - h.end.y)), DIM)
	draw_rect(Rect2(0, h.position.y, maxf(0.0, h.position.x), h.size.y), DIM)
	draw_rect(Rect2(h.end.x, h.position.y, maxf(0.0, full.size.x - h.end.x), h.size.y), DIM)
	# the pulse: a ring that breathes and a wider halo that fades out as it grows
	var k := 0.5 + 0.5 * sin(_t * 4.0) if not UiTokens.reduced_motion else 1.0
	var ring := StyleBoxFlat.new()
	ring.draw_center = false
	ring.border_color = Color(color, 0.6 + 0.4 * k)
	ring.set_border_width_all(4)
	ring.set_corner_radius_all(14)
	ring.shadow_color = Color(color, 0.30 * k)
	ring.shadow_size = 10
	draw_style_box(ring, h)
	if not UiTokens.reduced_motion:
		var grow := fposmod(_t * 1.4, 1.0)
		var halo := StyleBoxFlat.new()
		halo.draw_center = false
		halo.border_color = Color(color, 0.55 * (1.0 - grow))
		halo.set_border_width_all(3)
		halo.set_corner_radius_all(18)
		draw_style_box(halo, h.grow(6.0 + 26.0 * grow))
