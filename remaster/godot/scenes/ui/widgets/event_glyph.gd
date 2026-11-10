class_name EventGlyph
extends Control
## The glyph on a galactic event card: a per-kind mark (plague, supernova, rift, trade boom, derelict, unrest, breakthrough)
## inside a slowly turning orbit ring.

const KINDS := {"Plague": "ev_plague", "SupernovaWarning": "ev_supernova", "SubspaceRift": "ev_rift", "TradeBoom": "ev_trade",
	"DerelictVessel": "ev_derelict", "LaborUnrest": "ev_unrest", "ScientificBreakthrough": "ev_breakthrough"}

var kind := ""

func _process(_d: float) -> void:
	queue_redraw()

func _draw() -> void:
	var c := size / 2.0
	var t := Time.get_ticks_msec() / 1000.0
	var col := Lcars.accent2()
	draw_circle(c, 18, Color(col, 0.15))
	draw_arc(c, 19, t * 0.6, t * 0.6 + PI * 0.7, 16, Color(col, 0.8), 2.0, true)
	draw_arc(c, 19, t * 0.6 + PI, t * 0.6 + PI * 1.7, 16, Color(col, 0.8), 2.0, true)
	var key: String = KINDS.get(kind, "ev_breakthrough")
	UiIcon.draw_icon(self, key, Rect2(c - Vector2(12, 12), Vector2(24, 24)), col)
