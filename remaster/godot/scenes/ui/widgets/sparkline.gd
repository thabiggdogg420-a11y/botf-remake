class_name Sparkline
extends Control
## A small line chart of up to 20 values with an end dot (docs/UI-DESIGN-SYSTEM.md §5).

var values: Array = []
var color := Color.WHITE

static func make(vals: Array, c: Color, w: float = 120.0, h: float = 28.0) -> Sparkline:
	var s := Sparkline.new()
	s.values = vals.slice(maxi(0, vals.size() - 20))
	s.color = c
	s.custom_minimum_size = Vector2(w, h)
	s.mouse_filter = Control.MOUSE_FILTER_IGNORE
	return s

func _draw() -> void:
	if values.size() < 2:
		draw_line(Vector2(0, size.y / 2.0), Vector2(size.x, size.y / 2.0), Color(color, 0.4), 1.5)
		return
	var lo := float(values.min())
	var hi := float(values.max())
	var span: float = max(0.0001, hi - lo)
	var pts := PackedVector2Array()
	for n in values.size():
		pts.append(Vector2(size.x * n / float(values.size() - 1), size.y - 3.0 - (size.y - 6.0) * (float(values[n]) - lo) / span))
	var fill := pts.duplicate()
	fill.append(Vector2(size.x, size.y))
	fill.append(Vector2(0, size.y))
	draw_colored_polygon(fill, Color(color, 0.12))
	draw_polyline(pts, color, 2.0, true)
	draw_circle(pts[pts.size() - 1], 3.0, color)
