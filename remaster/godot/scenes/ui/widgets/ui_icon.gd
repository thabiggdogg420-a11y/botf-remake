class_name UiIcon
extends Control
## The UI icon set (docs/UI-DESIGN-SYSTEM.md §6): vector glyphs on a 24 px grid, 2 px stroke, drawn in code so they
## scale with UI scale and need no texture on either renderer. `UiIcon.draw_icon` draws one into any CanvasItem.

const NAMES := ["credits", "research", "industry", "food", "population", "intel", "morale", "energy", "growth", "ship",
	"fleet", "shield", "attack", "speed", "range", "hull", "build", "colony", "planet", "star", "galaxy", "system", "empire",
	"fleets", "diplomacy", "database", "treaty", "war", "peace", "trade", "alert", "warn", "good", "info", "clock", "turn",
	"search", "settings", "save", "menu", "events", "victory", "chronicle", "combat", "close", "plus", "minus", "up", "down",
	"arrow_right", "lock", "eye", "orders"]

var icon := "info"
var color := Color.WHITE

static func make(name_: String, px: float = 20.0, tint: Variant = null) -> UiIcon:
	var i := UiIcon.new()
	i.icon = name_
	i.custom_minimum_size = Vector2(px, px)
	i.color = Lcars.TEXT2 if tint == null else tint
	i.mouse_filter = Control.MOUSE_FILTER_IGNORE
	i.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	return i

func _draw() -> void:
	var s: float = min(size.x, size.y)
	draw_icon(self, icon, Rect2((size - Vector2(s, s)) / 2.0, Vector2(s, s)), color)

## Draws icon `name_` into `ci` inside `r` (a square) in `c`.
static func draw_icon(ci: CanvasItem, name_: String, r: Rect2, c: Color) -> void:
	var k := r.size.x / 24.0
	var o := r.position
	var w: float = max(1.25, 2.0 * k)
	var P := func(x: float, y: float) -> Vector2: return o + Vector2(x, y) * k
	var L := func(pts: Array) -> void:
		var a := PackedVector2Array()
		for p in pts:
			a.append(P.call(p[0], p[1]))
		ci.draw_polyline(a, c, w, true)
	var C := func(x: float, y: float, rad: float, a0: float = 0.0, a1: float = TAU) -> void:
		ci.draw_arc(P.call(x, y), rad * k, a0, a1, 24, c, w, true)
	var D := func(x: float, y: float, rad: float) -> void:
		ci.draw_circle(P.call(x, y), rad * k, c)
	match name_:
		"credits":
			C.call(12, 12, 9)
			L.call([[14.5, 8], [10.5, 8], [9, 9.5], [9, 14.5], [10.5, 16], [14.5, 16]])
			L.call([[12, 5.5], [12, 8]]); L.call([[12, 16], [12, 18.5]])
		"research":
			L.call([[9, 3], [9, 10], [4, 20], [20, 20], [15, 10], [15, 3]]); L.call([[7, 3], [17, 3]])
			L.call([[6.5, 15], [17.5, 15]])
		"industry":
			L.call([[3, 21], [3, 10], [9, 14], [9, 10], [15, 14], [15, 4], [20, 4], [20, 21], [3, 21]])
		"food":
			L.call([[12, 21], [12, 8]])
			for y in [8.0, 12.0, 16.0]:
				L.call([[12, y + 2], [8, y - 1]]); L.call([[12, y + 2], [16, y - 1]])
		"population":
			C.call(9, 8, 3.5); L.call([[3, 20], [3, 17], [6, 14], [12, 14], [15, 17], [15, 20]])
			C.call(17, 7, 2.8, -PI * 0.9, PI * 0.6); L.call([[17, 13], [20, 15], [21, 19]])
		"intel", "eye":
			L.call([[2, 12], [6, 7], [12, 5], [18, 7], [22, 12], [18, 17], [12, 19], [6, 17], [2, 12]]); C.call(12, 12, 3.5)
		"morale":
			C.call(12, 12, 9); D.call(9, 10, 1.2); D.call(15, 10, 1.2); C.call(12, 12, 5, PI * 0.2, PI * 0.8)
		"energy":
			L.call([[13, 2], [5, 14], [12, 14], [10, 22], [19, 9], [12, 9], [13, 2]])
		"growth", "up":
			L.call([[4, 18], [10, 12], [14, 15], [20, 7]]); L.call([[15, 7], [20, 7], [20, 12]])
		"down":
			L.call([[4, 6], [10, 12], [14, 9], [20, 17]]); L.call([[15, 17], [20, 17], [20, 12]])
		"ship", "combat":
			L.call([[3, 12], [10, 6], [21, 12], [10, 18], [3, 12]]); L.call([[10, 6], [7, 3]]); L.call([[10, 18], [7, 21]])
		"fleet", "fleets":
			L.call([[2, 9], [7, 6], [13, 9], [7, 12], [2, 9]]); L.call([[10, 16], [15, 13], [21, 16], [15, 19], [10, 16]])
		"shield":
			L.call([[12, 2], [20, 5], [20, 12], [12, 22], [4, 12], [4, 5], [12, 2]])
		"attack":
			C.call(12, 12, 8); L.call([[12, 1], [12, 7]]); L.call([[12, 17], [12, 23]]); L.call([[1, 12], [7, 12]]); L.call([[17, 12], [23, 12]])
		"speed":
			L.call([[3, 8], [13, 8]]); L.call([[6, 12], [18, 12]]); L.call([[3, 16], [13, 16]]); L.call([[15, 6], [21, 12], [15, 18]])
		"range":
			C.call(12, 12, 9); C.call(12, 12, 4.5); D.call(12, 12, 1.4)
		"hull":
			L.call([[4, 6], [20, 6], [20, 18], [4, 18], [4, 6]]); L.call([[4, 12], [20, 12]]); L.call([[12, 6], [12, 18]])
		"build", "orders":
			L.call([[4, 20], [14, 10]]); L.call([[12, 4], [20, 12], [17, 15], [9, 7], [12, 4]])
		"colony", "planet":
			C.call(12, 12, 6.5); ci.draw_arc(P.call(12, 12), 10.5 * k, -0.45, PI - 0.45, 24, c, w * 0.8, true)
		"star":
			L.call([[12, 2], [14.5, 9.5], [22, 12], [14.5, 14.5], [12, 22], [9.5, 14.5], [2, 12], [9.5, 9.5], [12, 2]])
		"galaxy":
			C.call(12, 12, 2.5)
			ci.draw_arc(P.call(12, 12), 6 * k, 0.2, PI + 0.6, 18, c, w, true)
			ci.draw_arc(P.call(12, 12), 9.5 * k, PI + 0.4, TAU + 0.9, 22, c, w, true)
		"system":
			D.call(12, 12, 2.5); C.call(12, 12, 6); C.call(12, 12, 10); D.call(18, 12, 1.6)
		"empire":
			L.call([[12, 2], [20, 7], [20, 17], [12, 22], [4, 17], [4, 7], [12, 2]]); L.call([[12, 7], [12, 17]])
		"diplomacy", "treaty":
			L.call([[2, 12], [6, 8], [10, 10], [14, 8], [18, 8], [22, 12]]); L.call([[6, 14], [10, 17], [13, 16], [16, 17], [19, 14]])
			L.call([[10, 10], [13, 13], [16, 11]])
		"database":
			ci.draw_arc(P.call(12, 6), 8 * k, 0, TAU, 24, c, w, true)
			L.call([[4, 6], [4, 18]]); L.call([[20, 6], [20, 18]])
			ci.draw_arc(P.call(12, 12), 8 * k, 0.1, PI - 0.1, 16, c, w, true)
			ci.draw_arc(P.call(12, 18), 8 * k, 0.1, PI - 0.1, 16, c, w, true)
		"war":
			L.call([[4, 4], [20, 20]]); L.call([[20, 4], [4, 20]]); L.call([[4, 8], [8, 4]]); L.call([[16, 4], [20, 8]])
		"peace", "good":
			L.call([[4, 13], [9, 18], [20, 6]])
		"trade":
			L.call([[3, 8], [18, 8]]); L.call([[14, 4], [18, 8], [14, 12]]); L.call([[21, 16], [6, 16]]); L.call([[10, 12], [6, 16], [10, 20]])
		"alert":
			L.call([[8, 2], [16, 2], [22, 8], [22, 16], [16, 22], [8, 22], [2, 16], [2, 8], [8, 2]]); L.call([[12, 7], [12, 13]]); D.call(12, 17, 1.3)
		"warn":
			L.call([[12, 3], [22, 20], [2, 20], [12, 3]]); L.call([[12, 9], [12, 14]]); D.call(12, 17, 1.2)
		"info":
			C.call(12, 12, 9.5); L.call([[12, 11], [12, 17]]); D.call(12, 7.5, 1.3)
		"clock", "turn":
			C.call(12, 12, 9.5); L.call([[12, 6], [12, 12], [16, 14]])
		"search":
			C.call(10, 10, 6.5); L.call([[15, 15], [21, 21]])
		"settings":
			C.call(12, 12, 3.5)
			for n in 8:
				var a := TAU * n / 8.0
				L.call([[12 + cos(a) * 6.5, 12 + sin(a) * 6.5], [12 + cos(a) * 9.5, 12 + sin(a) * 9.5]])
			C.call(12, 12, 6.5)
		"save":
			L.call([[4, 4], [17, 4], [20, 7], [20, 20], [4, 20], [4, 4]]); L.call([[8, 4], [8, 9], [15, 9], [15, 4]]); L.call([[7, 20], [7, 14], [17, 14], [17, 20]])
		"menu":
			L.call([[4, 6], [20, 6]]); L.call([[4, 12], [20, 12]]); L.call([[4, 18], [20, 18]])
		"events":
			L.call([[12, 2], [12, 6]]); C.call(12, 14, 7); L.call([[12, 10], [12, 14], [15, 16]])
			L.call([[4, 5], [7, 8]]); L.call([[20, 5], [17, 8]])
		"victory":
			L.call([[7, 3], [17, 3], [17, 9], [12, 14], [7, 9], [7, 3]]); L.call([[12, 14], [12, 18]]); L.call([[7, 21], [17, 21], [16, 18], [8, 18], [7, 21]])
			L.call([[7, 5], [3, 5], [4, 9], [8, 10]]); L.call([[17, 5], [21, 5], [20, 9], [16, 10]])
		"chronicle":
			L.call([[5, 3], [17, 3], [19, 5], [19, 21], [5, 21], [5, 3]]); L.call([[8, 8], [16, 8]]); L.call([[8, 12], [16, 12]]); L.call([[8, 16], [13, 16]])
		"close":
			L.call([[5, 5], [19, 19]]); L.call([[19, 5], [5, 19]])
		"plus":
			L.call([[12, 4], [12, 20]]); L.call([[4, 12], [20, 12]])
		"minus":
			L.call([[4, 12], [20, 12]])
		"arrow_right":
			L.call([[4, 12], [20, 12]]); L.call([[14, 6], [20, 12], [14, 18]])
		"lock":
			L.call([[5, 11], [19, 11], [19, 21], [5, 21], [5, 11]]); C.call(12, 9, 4.5, PI, TAU); L.call([[7.5, 9], [7.5, 11]]); L.call([[16.5, 9], [16.5, 11]])
		_:
			C.call(12, 12, 8)
