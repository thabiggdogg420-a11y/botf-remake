class_name PlanetOrb
extends Control
## A small drawn planet by class (M, L, O ... GasGiant, Asteroids).

var cls := "M"

const COLORS := {"M": Color("#4fae6a"), "L": Color("#7fa35a"), "O": Color("#3f7fd0"), "H": Color("#d0a060"), "K": Color("#9a8a80"), "J": Color("#d9a86a"), "GasGiant": Color("#d9a86a"), "Y": Color("#c0503a"), "N": Color("#b08040"), "D": Color("#80808a"), "Asteroids": Color("#707078")}

func _draw() -> void:
	var c: Color = COLORS.get(cls, Color("#8899aa"))
	var ctr := Vector2(size.x / 2.0, size.y / 2.0)
	var r: float = min(size.x, size.y) * 0.42
	if cls == "Asteroids":
		for k in range(9):
			var a := k * 0.7
			draw_circle(ctr + Vector2(cos(a) * r * 0.9, sin(a) * r * 0.35), 2.5 + (k % 3), c)
		return
	draw_circle(ctr, r + 3, Color(c, 0.18))
	draw_circle(ctr, r, c.darkened(0.35))
	draw_circle(ctr - Vector2(r * 0.18, r * 0.18), r * 0.78, c)
	draw_circle(ctr - Vector2(r * 0.35, r * 0.35), r * 0.3, Color(1, 1, 1, 0.25))
	if cls == "GasGiant" or cls == "J":
		draw_arc(ctr, r * 1.5, PI * 0.1, PI * 0.9, 20, Color(c.lightened(0.3), 0.6), 1.5, true)
