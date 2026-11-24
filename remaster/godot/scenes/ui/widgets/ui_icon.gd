class_name UiIcon
extends Control
## The UI icon set (docs/UI-DESIGN-SYSTEM.md §6): vector glyphs on a 24 px grid, drawn in code so they scale with UI scale and
## need no texture on either renderer. `UiIcon.draw_icon` draws one into any CanvasItem.
## M4 Trekify round 2: the set is Starfleet and LCARS, not web. The delta (the swept arrowhead of the combadge) replaces arrows
## and chevrons, a PADD stands for orders, logs and papers, the sensor sweep for search and range, isolinear chips for save
## and records, Okudagram bars for menus and intel, rank pips for morale, a dilithium crystal for energy, and so on.
## The stroke weight follows the interface look (UiTokens.icon_stroke): heavy for TOS, thin for the movies and Late.

const NAMES := ["credits", "research", "industry", "food", "population", "intel", "morale", "energy", "growth", "ship",
	"fleet", "shield", "attack", "speed", "range", "hull", "build", "colony", "planet", "star", "galaxy", "system", "empire",
	"fleets", "diplomacy", "database", "treaty", "war", "peace", "trade", "alert", "warn", "good", "info", "clock", "turn",
	"search", "settings", "save", "menu", "events", "victory", "chronicle", "combat", "close", "plus", "minus", "up", "down",
	"arrow_right", "lock", "eye", "orders", "tech_biotech", "tech_computers", "tech_construction", "tech_energy", "tech_propulsion",
	"tech_weapons", "ev_plague", "ev_supernova", "ev_rift", "ev_trade", "ev_derelict", "ev_unrest", "ev_breakthrough", "starbase", "contact",
	"delta_up", "delta_down", "delta_left", "delta_right", "tick", "cross", "pin", "pin_on", "minor", "sensor", "anomaly", "threat",
	"unexplored", "territory", "supply", "routes", "grid", "labels", "padd", "quadrant", "worker",
	"minor_mercantile", "minor_warlike", "minor_peaceful", "minor_xenophobic", "minor_spiritual", "minor_scientific"]
## The mark of a minor race by temperament (minors.json: Mercantile, Warlike, Peaceful, Xenophobic, Spiritual, Scientific).
static func minor_name(temperament: String) -> String:
	var k := "minor_" + temperament.to_lower()
	return k if NAMES.has(k) else "minor"

## The six research fields in core order (research_screen.gd field index).
const TECH_FIELDS := ["tech_biotech", "tech_computers", "tech_construction", "tech_energy", "tech_propulsion", "tech_weapons"]

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

## The Starfleet delta (the combadge's swept arrowhead) as a polygon: tip at -Y before `ang`, centred on `c`, radius `r`.
static func delta_points(c: Vector2, r: float, ang: float = 0.0) -> PackedVector2Array:
	var out := PackedVector2Array()
	for p in [Vector2(0, -1.0), Vector2(0.78, 0.95), Vector2(0, 0.5), Vector2(-0.78, 0.95)]:
		out.append(c + (p * r).rotated(ang))
	return out

## Draws the delta filled, or as an outline of width `w`.
static func draw_delta(ci: CanvasItem, c: Vector2, r: float, ang: float, col: Color, filled: bool = true, w: float = 2.0) -> void:
	var pts := delta_points(c, r, ang)
	if filled:
		ci.draw_colored_polygon(pts, col)
	else:
		var closed := pts.duplicate()
		closed.append(pts[0])
		ci.draw_polyline(closed, col, w, true)

## A broken ring: `n` arcs with gaps (the LCARS segmented ring) instead of a plain circle. `fill` is the share of each step drawn.
static func draw_seg_ring(ci: CanvasItem, c: Vector2, r: float, col: Color, w: float = 2.0, n: int = 8, fill: float = 0.72, rot: float = -0.3) -> void:
	# One draw_multiline for the whole ring (26 Nov perf pass): each draw_arc was two canvas draws, and the galaxy chart's rings
	# (a dozen arcs round each of 40-50 stars) came to ~2000 draws a frame, the main-thread cost behind the choppy web build.
	# The chords are the arcs' own six points each, so the ring is the same shape.
	var pts := PackedVector2Array()
	pts.resize(n * 10)
	var k := 0
	for i in n:
		var a0 := rot + TAU * i / n
		var span := TAU / n * fill
		var prev := c + Vector2(cos(a0), sin(a0)) * r
		for s in range(1, 6):
			var a := a0 + span * float(s) / 5.0
			var cur := c + Vector2(cos(a), sin(a)) * r
			pts[k] = prev
			pts[k + 1] = cur
			k += 2
			prev = cur
	ci.draw_multiline(pts, col, w, true)

## A target reticle: four corner brackets around `c` at half-size `r` (the selection mark in place of a plain circle).
static func draw_reticle(ci: CanvasItem, c: Vector2, r: float, col: Color, w: float = 2.0) -> void:
	var q := r * 0.58
	for sx in [-1.0, 1.0]:
		for sy in [-1.0, 1.0]:
			var corner := c + Vector2(sx * r, sy * r)
			ci.draw_line(corner, corner - Vector2(sx * q, 0.0), col, w)
			ci.draw_line(corner, corner - Vector2(0.0, sy * q), col, w)

## A console octagon (a clipped square): filled, with an outline.
static func draw_octagon(ci: CanvasItem, c: Vector2, r: float, fill: Color, line: Color, w: float = 1.5) -> void:
	var pts := PackedVector2Array()
	for k in 8:
		var a := PI / 8.0 + k * PI / 4.0
		pts.append(c + Vector2(cos(a), sin(a)) * r * 1.04)
	ci.draw_colored_polygon(pts, fill)
	var closed := pts.duplicate()
	closed.append(pts[0])
	ci.draw_polyline(closed, line, w, true)

## An LCARS bar: round cap on the left, square on the right (the Okudagram end), from x0 to x1 on the row centre cy.
static func draw_bar(ci: CanvasItem, x0: float, x1: float, cy: float, h: float, col: Color) -> void:
	ci.draw_rect(Rect2(x0 + h / 2.0, cy - h / 2.0, max(0.0, x1 - x0 - h / 2.0), h), col)
	ci.draw_circle(Vector2(x0 + h / 2.0, cy), h / 2.0, col)

## Trek icon pass (Simon, 18 Nov 2026): the status bar, rail, system screen and control icons are SVG from TrekIconSvg (generated by
## tools/art/build_trek_icons.py), rasterised once per name and pixel size and tinted; everything else falls back to the vector code below.
static var _svg_cache := {}

## The name's SVG key, or "" when the icon is still drawn in code.
static func svg_key(name_: String) -> String:
	var k: String = TrekIconSvg.ALIASES.get(name_, name_)
	return k if TrekIconSvg.SVG.has(k) else ""

## The white texture of an SVG icon at `px` device pixels (cached; null if the SVG module fails).
static func svg_texture(key: String, px: int) -> Texture2D:
	px = clampi(px, 8, 512)
	var ck := "%s@%d" % [key, px]
	if _svg_cache.has(ck):
		return _svg_cache[ck]
	var img := Image.new()
	var tex: Texture2D = null
	if img.load_svg_from_string(TrekIconSvg.SVG[key], float(px) / 24.0) == OK:
		img.generate_mipmaps()
		tex = ImageTexture.create_from_image(img)
	_svg_cache[ck] = tex
	return tex

## Draws icon `name_` into `ci` inside `r` (a square) in `c`.
static func draw_icon(ci: CanvasItem, name_: String, r: Rect2, c: Color) -> void:
	var sk := svg_key(name_)
	if sk != "":
		var sc: float = ci.get_global_transform_with_canvas().get_scale().x if ci.is_inside_tree() else 1.0
		var tex := svg_texture(sk, int(ceil(r.size.x * maxf(sc, 0.25))))
		if tex != null:
			ci.draw_texture_rect(tex, r, false, c)
			return
	var k := r.size.x / 24.0
	var o := r.position
	var w: float = max(1.25, UiTokens.icon_stroke() * k)
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
	var B := func(x0: float, y0: float, x1: float, y1: float) -> void:
		ci.draw_rect(Rect2(P.call(x0, y0), Vector2(x1 - x0, y1 - y0) * k), c)
	var DF := func(x: float, y: float, rad: float, ang: float = 0.0, filled: bool = true) -> void:
		draw_delta(ci, P.call(x, y), rad * k, ang, c, filled, w)
	var BAR := func(x0: float, x1: float, y: float, h: float) -> void:
		var a: Vector2 = P.call(x0, y)
		var b: Vector2 = P.call(x1, y)
		draw_bar(ci, a.x, b.x, a.y, h * k, c)
	var PADD := func() -> void:
		L.call([[5, 3], [16, 3], [19, 6], [19, 21], [5, 21], [5, 3]])
	match name_:
		"credits":
			# the credit chit: a card with a clipped corner, data stripes and a contact dot
			L.call([[3, 6], [17, 6], [21, 10], [21, 18], [3, 18], [3, 6]])
			L.call([[6, 10], [12, 10]]); L.call([[6, 14], [17, 14]]); D.call(17.5, 10, 1.1)
		"research":
			# a tricorder with its lid raised
			L.call([[4, 12], [20, 12], [20, 21], [4, 21], [4, 12]]); L.call([[5.5, 12], [7.5, 4], [16.5, 4], [18.5, 12]])
			L.call([[9, 8], [15, 8]]); L.call([[7, 15.5], [13, 15.5], [13, 18.5], [7, 18.5], [7, 15.5]]); C.call(17, 17, 1.6)
		"industry":
			# a replicator alcove with its pattern ring
			L.call([[3, 21], [3, 9], [8, 4], [16, 4], [21, 9], [21, 21], [3, 21]]); C.call(12, 13, 3.8); L.call([[6, 18], [18, 18]])
		"food":
			L.call([[12, 21], [12, 8]])
			for y in [8.0, 12.0, 16.0]:
				L.call([[12, y + 2], [8, y - 1]]); L.call([[12, y + 2], [16, y - 1]])
		"population":
			# a crew member with the badge on the chest
			C.call(12, 7, 3.4); L.call([[4, 21], [4, 17.5], [7.5, 13.5], [16.5, 13.5], [20, 17.5], [20, 21]]); DF.call(12, 18, 2.8, 0.0, false)
		"intel":
			# Okudagram bars
			BAR.call(3, 21, 6, 2.8); BAR.call(3, 15, 12, 2.8); B.call(17, 10.6, 21, 13.4); BAR.call(3, 19, 18, 2.8)
		"morale":
			# rank pips in a pill
			C.call(7, 12, 5, PI / 2.0, PI * 1.5); C.call(17, 12, 5, -PI / 2.0, PI / 2.0); L.call([[7, 7], [17, 7]]); L.call([[7, 17], [17, 17]])
			D.call(8, 12, 1.5); D.call(12, 12, 1.5); D.call(16, 12, 1.5)
		"energy":
			# a dilithium crystal
			L.call([[12, 2], [17, 7], [17, 16], [12, 22], [7, 16], [7, 7], [12, 2]]); L.call([[7, 7], [12, 10], [17, 7]]); L.call([[12, 10], [12, 22]])
		"growth", "up", "delta_up":
			DF.call(12, 12.5, 9.5, 0.0)
		"down", "delta_down":
			DF.call(12, 11.5, 9.5, PI)
		"delta_left":
			DF.call(12, 12, 9.5, -PI / 2.0)
		"arrow_right", "delta_right":
			DF.call(12, 12, 9.5, PI / 2.0)
		"ship":
			ShipGlyph.draw_hull(ci, "saucer", o + Vector2(12, 12) * k, 23.0 * k, 0.0, c, false, true, 0.0)
		"fleet", "fleets":
			ShipGlyph.draw_hull(ci, "saucer", o + Vector2(10, 7.5) * k, 17.0 * k, 0.0, c, false, true, 0.0)
			ShipGlyph.draw_hull(ci, "saucer", o + Vector2(14, 16.5) * k, 17.0 * k, 0.0, c, false, true, 0.0)
		"starbase":
			ShipGlyph.draw_station(ci, o + Vector2(12, 12) * k, 9.0 * k, c, true)
		"contact":
			ShipGlyph.draw_contact(ci, o + Vector2(12, 12) * k, 7.0 * k, c, 0, "")
		"combat", "tech_weapons":
			L.call([[3, 10], [13, 10], [19, 12], [13, 14], [3, 14], [3, 10]]); L.call([[3, 10], [1, 7]]); L.call([[3, 14], [1, 17]])
			L.call([[16, 6], [22, 4]]); L.call([[17, 18], [23, 20]]); L.call([[20, 12], [23, 12]]); L.call([[6, 12], [11, 12]])
		"tech_biotech":
			var pa := []
			var pb := []
			for n in 13:
				var yy := 2.0 + n * 20.0 / 12.0
				pa.append([12.0 + 6.0 * sin(n * 0.55), yy])
				pb.append([12.0 - 6.0 * sin(n * 0.55), yy])
			L.call(pa); L.call(pb)
			for n in [1, 3, 5, 7, 9, 11]:
				L.call([pa[n], pb[n]])
		"tech_computers":
			L.call([[6, 6], [18, 6], [18, 18], [6, 18], [6, 6]]); L.call([[9.5, 9.5], [14.5, 9.5], [14.5, 14.5], [9.5, 14.5], [9.5, 9.5]])
			for q in [9.0, 12.0, 15.0]:
				L.call([[q, 3], [q, 6]]); L.call([[q, 18], [q, 21]]); L.call([[3, q], [6, q]]); L.call([[18, q], [21, q]])
		"tech_construction", "build":
			L.call([[3, 5], [21, 5], [21, 19], [3, 19], [3, 5]]); L.call([[3, 19], [12, 5], [21, 19]]); L.call([[3, 5], [12, 19], [21, 5]])
			L.call([[3, 12], [21, 12]])
		"tech_energy":
			# a warp core: the column, its bands and the two injector arms
			L.call([[9, 3], [15, 3], [15, 21], [9, 21], [9, 3]])
			for y in [7.0, 12.0, 17.0]:
				L.call([[9, y], [15, y]])
			L.call([[9, 12], [4, 12]]); L.call([[15, 12], [20, 12]]); D.call(4, 12, 1.4); D.call(20, 12, 1.4)
		"tech_propulsion":
			L.call([[2, 9], [13, 8], [18, 12], [13, 16], [2, 15], [2, 9]]); L.call([[5, 12], [13, 12]])
			ci.draw_arc(P.call(15, 12), 4.5 * k, -1.1, 1.1, 10, c, w * 0.8, true); ci.draw_arc(P.call(15, 12), 8.0 * k, -1.0, 1.0, 12, c, w * 0.8, true)
			L.call([[2, 4], [9, 4]]); L.call([[2, 20], [9, 20]])
		"ev_plague":
			C.call(12, 12, 3.0)
			for n in 3:
				var a1 := -PI / 2.0 + TAU * n / 3.0
				ci.draw_arc(P.call(12 + cos(a1) * 6.0, 12 + sin(a1) * 6.0), 4.8 * k, a1 - 2.4, a1 + 2.4, 12, c, w, true)
			C.call(12, 12, 10.0)
		"ev_supernova":
			D.call(12, 12, 3.0)
			for n in 8:
				var a2 := TAU * n / 8.0
				var r1 := 5.0
				var r2 := 11.0 if n % 2 == 0 else 8.0
				L.call([[12 + cos(a2) * r1, 12 + sin(a2) * r1], [12 + cos(a2) * r2, 12 + sin(a2) * r2]])
		"ev_rift":
			L.call([[12, 1], [9, 7], [14, 11], [9, 16], [13, 23]])
			ci.draw_arc(P.call(12, 12), 8.5 * k, 2.2, 4.1, 10, c, w * 0.8, true); ci.draw_arc(P.call(12, 12), 8.5 * k, -0.95, 0.95, 10, c, w * 0.8, true)
		"ev_trade", "trade":
			# gold-pressed latinum: two slabs
			L.call([[2, 20], [5, 15], [17, 15], [14, 20], [2, 20]]); L.call([[8, 13], [11, 8], [22, 8], [19, 13], [8, 13]])
		"ev_derelict":
			ShipGlyph.draw_hull(ci, "saucer", o + Vector2(12, 12) * k, 20.0 * k, -0.45, c, false, true, 0.0)
			L.call([[3, 4], [21, 20]])
		"ev_unrest", "alert":
			# the alert beacon: a dome on its base with its rays
			ci.draw_arc(P.call(12, 16), 7.0 * k, PI, TAU, 14, c, w, true); L.call([[5, 16], [19, 16]]); L.call([[3, 20], [21, 20]])
			L.call([[12, 3], [12, 6]]); L.call([[4.5, 5.5], [6.5, 8]]); L.call([[19.5, 5.5], [17.5, 8]]); L.call([[12, 10], [12, 13]])
		"ev_breakthrough":
			D.call(12, 12, 2.0); C.call(12, 12, 9.0); ci.draw_arc(P.call(12, 12), 4.5 * k, 0, TAU, 16, c, w * 0.8, true)
			L.call([[12, 1], [12, 4]]); L.call([[12, 20], [12, 23]]); L.call([[1, 12], [4, 12]]); L.call([[20, 12], [23, 12]])
		"shield":
			# the deflector bubble over a hull
			ci.draw_arc(P.call(12, 16), 10.0 * k, PI, TAU, 20, c, w, true); ci.draw_arc(P.call(12, 16), 6.0 * k, PI * 1.1, TAU - PI * 0.1, 14, c, w * 0.8, true)
			DF.call(12, 18.5, 3.2, 0.0, false)
		"attack":
			# targeting brackets with a diamond lock
			L.call([[3, 8], [3, 3], [8, 3]]); L.call([[16, 3], [21, 3], [21, 8]]); L.call([[21, 16], [21, 21], [16, 21]]); L.call([[8, 21], [3, 21], [3, 16]])
			L.call([[12, 7], [17, 12], [12, 17], [7, 12], [12, 7]]); D.call(12, 12, 1.6)
		"speed":
			# warp: three streaks and the bow shock
			L.call([[2, 8], [11, 8]]); L.call([[2, 12], [8, 12]]); L.call([[2, 16], [11, 16]])
			ci.draw_arc(P.call(13, 12), 4.5 * k, -1.1, 1.1, 10, c, w, true); ci.draw_arc(P.call(13, 12), 9.0 * k, -1.0, 1.0, 14, c, w, true)
		"range", "sensor", "eye":
			# the sensor sweep: range arcs, the sweep line and a contact
			ci.draw_arc(P.call(4, 20), 6.0 * k, -PI / 2.0, 0.0, 10, c, w, true); ci.draw_arc(P.call(4, 20), 11.0 * k, -PI / 2.0, 0.0, 14, c, w, true)
			ci.draw_arc(P.call(4, 20), 16.0 * k, -PI / 2.0, 0.0, 18, c, w, true); L.call([[4, 20], [14.3, 7.7]]); D.call(17, 14, 1.5)
		"hull":
			# aztec plating
			L.call([[3, 5], [21, 5], [21, 19], [3, 19], [3, 5]]); L.call([[3, 12], [21, 12]]); L.call([[12, 5], [12, 12]])
			L.call([[7.5, 12], [7.5, 19]]); L.call([[16.5, 12], [16.5, 19]])
		"colony":
			# a world with the Federation's delta planted on it
			C.call(10, 14, 6.5); ci.draw_arc(P.call(10, 8.5), 6.5 * k, 0.55, 2.59, 10, c, w * 0.8, true); DF.call(19, 6.5, 4.4, 0.0)
		"planet":
			C.call(12, 12, 6.5); ci.draw_arc(P.call(12, 7), 6.5 * k, 0.55, 2.59, 10, c, w * 0.8, true)
			ci.draw_arc(P.call(12, 12), 10.5 * k, -0.45, PI - 0.45, 24, c, w * 0.8, true); D.call(20.5, 15.5, 1.5)
		"star":
			# a stellar chart mark: core, ring and the cross hairs
			D.call(12, 12, 3.0); C.call(12, 12, 6.5)
			L.call([[12, 1], [12, 4.5]]); L.call([[12, 19.5], [12, 23]]); L.call([[1, 12], [4.5, 12]]); L.call([[19.5, 12], [23, 12]])
		"galaxy", "quadrant":
			# the quadrant chart, our position marked in the Alpha quadrant
			C.call(12, 12, 10); L.call([[12, 2], [12, 22]]); L.call([[2, 12], [22, 12]]); D.call(17.5, 6.5, 2.0)
		"system":
			D.call(12, 12, 2.5); C.call(12, 12, 6); C.call(12, 12, 10); D.call(18, 12, 1.6)
		"empire":
			# the Federation seal in brief: ring, laurels and a star
			C.call(12, 12, 9.8); ci.draw_arc(P.call(12, 12), 6.6 * k, 2.2, 4.1, 10, c, w * 0.9, true); ci.draw_arc(P.call(12, 12), 6.6 * k, -0.95, 0.95, 10, c, w * 0.9, true)
			L.call([[12, 8], [13, 11], [16, 12], [13, 13], [12, 16], [11, 13], [8, 12], [11, 11], [12, 8]])
		"diplomacy":
			# a combadge opening a channel
			DF.call(7.5, 13, 6.2, 0.0, false)
			for rr in [4.0, 7.5, 11.0]:
				ci.draw_arc(P.call(12.5, 11.5), rr * k, -0.8, 0.8, 8, c, w * 0.9, true)
		"treaty":
			# a signed accord: the page with its seal
			L.call([[5, 3], [16, 3], [19, 6], [19, 21], [5, 21], [5, 3]]); L.call([[8, 8], [16, 8]]); L.call([[8, 11.5], [16, 11.5]]); C.call(12, 17, 2.6)
		"database":
			# isolinear data bars
			for y in [3.0, 9.5, 16.0]:
				L.call([[3, y], [21, y], [21, y + 5], [3, y + 5], [3, y]]); B.call(5, y + 1.5, 8, y + 3.5)
		"war":
			# crossed lines of fire with a burst
			L.call([[5, 5], [19, 19]]); L.call([[19, 5], [5, 19]]); D.call(4.5, 4.5, 1.7); D.call(19.5, 4.5, 1.7); D.call(4.5, 19.5, 1.7); D.call(19.5, 19.5, 1.7)
			C.call(12, 12, 3.4)
		"peace", "good":
			C.call(8, 12, 6.0, PI / 2.0, PI * 1.5); C.call(16, 12, 6.0, -PI / 2.0, PI / 2.0); L.call([[8, 6], [16, 6]]); L.call([[8, 18], [16, 18]])
			L.call([[8.5, 12], [11, 14.5], [15.5, 9.5]])
		"tick":
			L.call([[4, 13], [9, 18], [20, 6]])
		"cross", "close":
			L.call([[5, 5], [19, 19]]); L.call([[19, 5], [5, 19]])
		"warn":
			# the delta with its mark
			DF.call(12, 12, 10.0, 0.0, false); L.call([[12, 8.5], [12, 12.8]]); D.call(12, 15.2, 1.1)
		"info":
			# the bracketed readout, as the movie-era headers frame a title
			L.call([[7, 3], [3, 3], [3, 21], [7, 21]]); L.call([[17, 3], [21, 3], [21, 21], [17, 21]]); D.call(12, 8, 1.4); L.call([[12, 11], [12, 17]])
		"clock", "turn":
			# a chronometer ring with its pointer
			C.call(12, 12, 9.5, -PI / 2.0 + 0.4, PI * 1.5 - 0.4); L.call([[12, 12], [16, 8.5]]); D.call(12, 12, 1.4); L.call([[12, 1.5], [12, 4]])
		"search":
			# the sensor sweep in a ring
			C.call(12, 12, 9.0); C.call(12, 12, 4.5, -0.4, 2.8); L.call([[12, 12], [19, 5.5]]); D.call(8, 8, 1.4); D.call(15.5, 16, 1.2)
		"settings":
			# control sliders with square knobs
			for row in [[6.0, 16.0], [12.0, 8.0], [18.0, 14.0]]:
				L.call([[3, row[0]], [21, row[0]]]); B.call(row[1] - 1.8, row[0] - 2.6, row[1] + 1.8, row[0] + 2.6)
		"save":
			# an isolinear chip
			L.call([[5, 3], [17, 3], [19, 5], [19, 21], [5, 21], [5, 3]]); L.call([[8, 6], [16, 6], [16, 12], [8, 12], [8, 6]])
			L.call([[8, 15], [8, 19]]); L.call([[12, 15], [12, 19]]); L.call([[16, 15], [16, 19]])
		"menu":
			BAR.call(3, 21, 6, 2.8); BAR.call(3, 15, 12, 2.8); BAR.call(3, 19, 18, 2.8)
		"events":
			# an incoming subspace transmission
			L.call([[12, 21], [12, 10.5]]); L.call([[8, 21], [16, 21]]); D.call(12, 9, 1.6)
			for rr in [4.5, 8.0, 11.5]:
				ci.draw_arc(P.call(12, 9), rr * k, -2.25, -0.9, 8, c, w * 0.9, true)
		"victory":
			# a ribbon bar and its star
			L.call([[4, 3], [20, 3], [20, 11], [4, 11], [4, 3]]); L.call([[9.3, 3], [9.3, 11]]); L.call([[14.7, 3], [14.7, 11]])
			L.call([[12, 13], [13.3, 16.2], [16.5, 17.5], [13.3, 18.8], [12, 22], [10.7, 18.8], [7.5, 17.5], [10.7, 16.2], [12, 13]])
		"chronicle":
			# the captain's log PADD
			PADD.call(); L.call([[8, 10], [16, 10]]); L.call([[8, 13.5], [16, 13.5]]); L.call([[8, 17], [13, 17]]); D.call(8.5, 6.3, 1.2)
		"orders", "padd":
			PADD.call(); L.call([[7.5, 9], [9, 10.5], [11.5, 7.5]]); L.call([[13, 9], [16, 9]]); L.call([[7.5, 15], [9, 16.5], [11.5, 13.5]]); L.call([[13, 15], [16, 15]])
		"lock":
			# an airlock iris
			C.call(12, 12, 9.5); C.call(12, 12, 3.5)
			for n in 6:
				var a3 := TAU * n / 6.0
				L.call([[12 + cos(a3) * 3.5, 12 + sin(a3) * 3.5], [12 + cos(a3 + 0.55) * 9.5, 12 + sin(a3 + 0.55) * 9.5]])
		"plus":
			L.call([[12, 4], [12, 20]]); L.call([[4, 12], [20, 12]])
		"minus":
			L.call([[4, 12], [20, 12]])
		"pin":
			DF.call(12, 12, 8.5, 0.0, false)
		"pin_on":
			DF.call(12, 12, 8.5, 0.0, true)
		"minor":
			# an independent world: the same mark the chart uses
			Insignia.draw(ci, "minor", r, c, "small")
		"minor_mercantile":
			C.call(12, 12, 10.0); L.call([[5, 15], [7.5, 11], [15.5, 11], [13, 15], [5, 15]]); L.call([[9, 9.5], [11, 6], [19, 6], [17, 9.5], [9, 9.5]])
		"minor_warlike":
			C.call(12, 12, 10.0); L.call([[7, 7], [17, 17]]); L.call([[17, 7], [7, 17]]); D.call(12, 12, 1.8)
		"minor_peaceful":
			C.call(12, 12, 10.0); ci.draw_arc(P.call(12, 12), 5.5 * k, 2.1, 4.2, 8, c, w, true); ci.draw_arc(P.call(12, 12), 5.5 * k, -1.05, 1.05, 8, c, w, true); D.call(12, 12, 1.8)
		"minor_xenophobic":
			C.call(12, 12, 10.0); C.call(12, 12, 3.2)
			for n in 6:
				var a5 := TAU * n / 6.0
				L.call([[12 + cos(a5) * 3.2, 12 + sin(a5) * 3.2], [12 + cos(a5 + 0.6) * 8.0, 12 + sin(a5 + 0.6) * 8.0]])
		"minor_spiritual":
			C.call(12, 12, 10.0); D.call(12, 12, 2.2)
			for n in 8:
				var a6 := TAU * n / 8.0
				L.call([[12 + cos(a6) * 4.2, 12 + sin(a6) * 4.2], [12 + cos(a6) * 7.2, 12 + sin(a6) * 7.2]])
		"minor_scientific":
			C.call(12, 12, 10.0); D.call(12, 12, 1.8)
			ci.draw_arc(P.call(12, 12), 6.5 * k, 0.4, 2.6, 10, c, w * 0.9, true); ci.draw_arc(P.call(12, 12), 6.5 * k, 3.5, 5.7, 10, c, w * 0.9, true)
		"anomaly":
			ci.draw_arc(P.call(12, 12), 3.5 * k, 0.0, 4.5, 10, c, w, true); ci.draw_arc(P.call(12, 12), 7.0 * k, 2.0, 6.5, 14, c, w, true); ci.draw_arc(P.call(12, 12), 10.5 * k, 4.0, 8.3, 18, c, w, true)
		"threat":
			DF.call(12, 11.5, 9.5, PI, false); D.call(12, 12, 1.8)
		"unexplored":
			for n in 8:
				var a4 := TAU * n / 8.0
				ci.draw_arc(P.call(12, 12), 8.5 * k, a4, a4 + TAU / 8.0 * 0.55, 4, c, w, true)
		"territory":
			L.call([[3, 8], [9, 3], [18, 5], [21, 12], [16, 20], [7, 19], [3, 8]]); D.call(12, 12, 1.8)
		"supply":
			for n in 3:
				ci.draw_arc(P.call(12, 12), 9.0 * k, TAU * n / 3.0 + 0.2, TAU * (n + 1) / 3.0 - 0.2, 8, c, w, true)
			DF.call(12, 12.5, 4.0, 0.0)
		"routes":
			L.call([[3, 19], [8, 13], [14, 15]]); DF.call(18.5, 8.5, 4.6, 0.9)
		"grid":
			L.call([[3, 8], [21, 8]]); L.call([[3, 16], [21, 16]]); L.call([[8, 3], [8, 21]]); L.call([[16, 3], [16, 21]])
		"labels":
			BAR.call(3, 15, 7, 2.6); BAR.call(3, 21, 12, 2.6); BAR.call(3, 11, 17, 2.6)
		_:
			C.call(12, 12, 8)
