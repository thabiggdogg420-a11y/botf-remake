class_name EventGlyph
extends Control
## The glyph on a galactic event card: a per-kind mark (plague, supernova, rift, trade boom, derelict, unrest, breakthrough)
## on an LCARS priority plate (an octagon with corner brackets), not a spinning web-style ring.

const KINDS := {"Plague": "ev_plague", "SupernovaWarning": "ev_supernova", "SubspaceRift": "ev_rift", "TradeBoom": "ev_trade",
	"DerelictVessel": "ev_derelict", "LaborUnrest": "ev_unrest", "ScientificBreakthrough": "ev_breakthrough"}

var kind := ""

func _draw() -> void:
	var c := size / 2.0
	var col := Lcars.accent2()
	var oct := PackedVector2Array()
	for n in 8:
		var a := PI / 8.0 + TAU * n / 8.0
		oct.append(c + Vector2(cos(a), sin(a)) * 21.0)
	draw_colored_polygon(oct, Color(col, 0.14))
	var closed := oct.duplicate()
	closed.append(oct[0])
	draw_polyline(closed, Color(col, 0.55), 1.5, true)
	for q in [Vector2(-1, -1), Vector2(1, -1), Vector2(1, 1), Vector2(-1, 1)]:
		var corner: Vector2 = c + q * 22.0
		draw_line(corner, corner - Vector2(q.x * 7.0, 0), col, 2.0)
		draw_line(corner, corner - Vector2(0, q.y * 7.0), col, 2.0)
	var key: String = KINDS.get(kind, "ev_breakthrough")
	UiIcon.draw_icon(self, key, Rect2(c - Vector2(12, 12), Vector2(24, 24)), col)
