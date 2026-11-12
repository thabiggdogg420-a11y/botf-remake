class_name LcarsElbow
extends Control
## The console corner (top-left): the rail column meeting the top bar, with the faction emblem. The shape follows the interface look:
## the sweeping LCARS elbow (TNG, Voyager, Late), the boxed push-button console of the original series (TOS), and the flat chamfered
## grey-blue plate of the movie era.

var empire := 1

func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE

func _draw() -> void:
	var a := Lcars.accent()
	var w := App.RAIL_W
	var h := App.TOP_H - 14
	var era := str(UiTokens.era)
	var fill := a
	if era == "tos":
		fill = UiTokens.era_hi()
	elif era == "movie":
		fill = UiTokens.era_hi()
	var ink := Lcars.ink()
	match era:
		"tos":
			# The boxed console: a square L of gold with its lamp row, no sweep.
			draw_rect(Rect2(0, 0, w, h + 30), fill)
			draw_rect(Rect2(w, 0, 40, h), fill)
			for k in 3:
				draw_rect(Rect2(w + 6 + k * 11, h - 14, 8, 8), [UiTokens.era_lo(), Color("#6fd98f"), Color("#7fb3ff")][k])
		"movie":
			# The flat plate: a thin L with a 45 degree clip at the inner corner.
			draw_rect(Rect2(0, 0, w, h + 30), fill)
			draw_rect(Rect2(w, 0, 40, h - 22), fill)
			var clip := PackedVector2Array([Vector2(w, h - 22), Vector2(w + 40, h - 22), Vector2(w + 18, h), Vector2(w, h)])
			draw_colored_polygon(clip, fill)
			draw_rect(Rect2(w, h, 30, 30), fill)
			draw_colored_polygon(PackedVector2Array([Vector2(w, h + 30), Vector2(w + 30, h + 30), Vector2(w, h)]), Lcars.BG)
			draw_line(Vector2(w + 44, h - 22), Vector2(w + 44, 0), Lcars.BG, 3.0)
		_:
			draw_rect(Rect2(0, 0, w, h + 30), fill)
			draw_rect(Rect2(w, 0, 40, h), fill)
			draw_rect(Rect2(w, h, 30, 30), fill)
			# inner corner cut
			var pts := PackedVector2Array()
			for k in range(0, 17):
				var ang := PI + PI / 2.0 * k / 16.0
				pts.append(Vector2(w + 30, h + 30) + Vector2(cos(ang), sin(ang)) * 30)
			pts.append(Vector2(w + 30, h + 30))
			draw_colored_polygon(pts, Lcars.BG)
	Lcars.draw_emblem(self, empire, Vector2(34, 34), 44, ink, false, fill)
	# The label is LCARS only for the Federation in the LCARS looks; the TOS console names the service, the movie plate its command.
	var label := "LCARS" if empire == 1 and Lcars.faction_short(1) == "Federation" else Lcars.faction_short(empire).to_upper()
	if empire == 1 and Lcars.faction_short(1) == "Federation":
		if era == "tos":
			label = "STARFLEET"
		elif era == "movie":
			label = "FLEET OPS"
	draw_string(Lcars.font("display", 600), Vector2(62, 44), label, HORIZONTAL_ALIGNMENT_LEFT, -1, 22 if label.length() < 8 else 18, ink)
