class_name ChoiceIcon
extends Control
## Construction choice glyph: a hull arrow for ships, a block for structures.

var product := ""
var ship := false

func _draw() -> void:
	var c := size / 2.0
	var a := Lcars.accent()
	draw_circle(c, 16, Color(a, 0.15))
	if ship:
		draw_colored_polygon(PackedVector2Array([c + Vector2(0, -11), c + Vector2(8, 9), c + Vector2(0, 5), c + Vector2(-8, 9)]), a)
	else:
		draw_rect(Rect2(c - Vector2(8, 6), Vector2(16, 12)), a, false, 2.0)
		draw_rect(Rect2(c - Vector2(3, 11), Vector2(6, 5)), a)
