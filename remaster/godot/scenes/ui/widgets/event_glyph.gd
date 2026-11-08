class_name EventGlyph
extends Control
## The spinning glyph on a galactic event card.

var kind := ""
func _process(_d: float) -> void:
	queue_redraw()
func _draw() -> void:
	var c := size / 2.0
	var t := Time.get_ticks_msec() / 1000.0
	var col := Lcars.accent2()
	draw_circle(c, 18, Color(col, 0.15))
	draw_arc(c, 17, t, t + PI * 1.4, 24, col, 2.0, true)
	draw_arc(c, 11, -t * 1.3, -t * 1.3 + PI, 24, Color(col, 0.7), 2.0, true)
	draw_circle(c, 4, col)
