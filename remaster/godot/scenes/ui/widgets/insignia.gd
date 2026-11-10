class_name Insignia
extends RefCounted
## Empire and power insignia, drawn in-house as vector paths (no image files, nothing copied from anywhere).
## One draw API serves every emblem site in the UI (Lcars.draw_emblem / Lcars.emblem route here):
##
##   Insignia.draw(canvas_item, empire, rect, colour, style = "auto", bg = null)
##
## empire: 0..5 (neutral, Federation, Klingon, Romulan, Cardassian, Ferengi) or a key: "dominion", "borg", "species8472",
##   "breen", "hirogen", "kazon", "xindi", "krenim", "tholian", "gorn", "orion", "maquis", "devore", "minor", "neutral"
##   (roster faction names such as "Dominion" work too). Unknown keys draw the neutral mark.
## style: "auto" (the small variant under SMALL_PX), "full", "small", "outline".
## bg: colour of the cut-outs and halos (default Lcars.BG); pass the surface the emblem sits on.
## Every mark is one colour plus cut-outs, drawn in a 0..100 box. Each has a heavier small variant for 14 px use.

const BY_ID := ["neutral", "federation", "klingon", "romulan", "cardassian", "ferengi"]
const ALL := ["federation", "klingon", "romulan", "cardassian", "ferengi", "dominion", "borg", "species8472", "breen", "hirogen",
	"kazon", "xindi", "krenim", "tholian", "gorn", "orion", "maquis", "devore", "minor", "neutral"]
const ALIASES := {"fed": "federation", "starfleet": "federation", "ufp": "federation", "klingons": "klingon", "romulans": "romulan",
	"reman": "romulan", "cardassians": "cardassian", "ferengis": "ferengi", "jemhadar": "dominion", "8472": "species8472",
	"species 8472": "species8472", "unknown": "neutral", "contact": "neutral"}
const SMALL_PX := 36.0
## Marks whose full variant stays legible lower down switch to the small variant later.
const SMALL_BELOW := {"federation": 26.0}

static var _cache := {}

static func key_of(empire: Variant) -> String:
	if empire is int or empire is float:
		var i := int(empire)
		return BY_ID[i] if i >= 0 and i < BY_ID.size() else "neutral"
	var s := String(empire).to_lower().strip_edges()
	if ALIASES.has(s):
		s = ALIASES[s]
	return s if ALL.has(s) else "neutral"

## Draws the insignia into `ci` inside `rect` (kept square, centred).
static func draw(ci: CanvasItem, empire: Variant, rect: Rect2, colour: Color, style: String = "auto", bg: Variant = null) -> void:
	var key := key_of(empire)
	var px := minf(rect.size.x, rect.size.y)
	if px < 1.0:
		return
	var cut_px: float = SMALL_BELOW.get(key, SMALL_PX)
	var small := style == "small" or (style == "auto" and px < cut_px) or (style == "outline" and px < cut_px)
	var ops: Array = ops_for(key, small)
	var k := px / 100.0
	var o := rect.position + (rect.size - Vector2(px, px)) / 2.0
	var bgc: Color = Lcars.BG if bg == null else bg
	var outline := style == "outline"
	for op in ops:
		var kind: String = op[0]
		var src: PackedVector2Array = op[1]
		var pts := PackedVector2Array()
		for p in src:
			pts.append(o + p * k)
		var w: float = maxf(1.2, float(op[2]) * k) if op.size() > 2 else 1.0
		match kind:
			"f":
				if outline:
					_stroke(ci, pts, colour, maxf(1.5, 3.0 * k), true)
				else:
					_fill(ci, pts, colour)
			"c":
				if not outline:
					_fill(ci, pts, bgc)
			"h":
				if not outline:
					_stroke(ci, pts, bgc, w, true)
			"s":
				_stroke(ci, pts, colour, w, false)
			"S":
				_stroke(ci, pts, colour, w, true)
			"x":
				if not outline:
					_stroke(ci, pts, bgc, w, false)
			"X":
				if not outline:
					_stroke(ci, pts, bgc, w, true)

static func _fill(ci: CanvasItem, pts: PackedVector2Array, col: Color) -> void:
	if pts.size() < 3:
		return
	if not Geometry2D.triangulate_polygon(pts).is_empty():
		ci.draw_colored_polygon(pts, col)
	var loop := pts.duplicate()
	loop.append(pts[0])
	ci.draw_polyline(loop, col, 1.0, true)

static func _stroke(ci: CanvasItem, pts: PackedVector2Array, col: Color, w: float, closed: bool) -> void:
	if pts.size() < 2:
		return
	var l := pts
	if closed:
		l = pts.duplicate()
		l.append(pts[0])
	ci.draw_polyline(l, col, w, true)

static func ops_for(key: String, small: bool) -> Array:
	var ck := "%s/%d" % [key, 1 if small else 0]
	if not _cache.has(ck):
		_cache[ck] = _build(key, small)
	return _cache[ck]

# ---------------------------------------------------------------------------------------------
# Geometry helpers (0..100 box)
# ---------------------------------------------------------------------------------------------
static func _bez(a: Vector2, b: Vector2, c: Vector2, d: Vector2, n: int = 14) -> PackedVector2Array:
	var out := PackedVector2Array()
	for i in range(n + 1):
		var t := float(i) / n
		var u := 1.0 - t
		out.append(a * u * u * u + b * 3.0 * u * u * t + c * 3.0 * u * t * t + d * t * t * t)
	return out

static func _quad(a: Vector2, b: Vector2, c: Vector2, n: int = 10) -> PackedVector2Array:
	var out := PackedVector2Array()
	for i in range(n + 1):
		var t := float(i) / n
		var u := 1.0 - t
		out.append(a * u * u + b * 2.0 * u * t + c * t * t)
	return out

static func _circle(c: Vector2, r: float, n: int = 36) -> PackedVector2Array:
	var out := PackedVector2Array()
	for i in range(n):
		var a := TAU * i / n
		out.append(c + Vector2(cos(a), sin(a)) * r)
	return out

static func _ellipse(c: Vector2, rx: float, ry: float, rot_deg: float, n: int = 40) -> PackedVector2Array:
	var out := PackedVector2Array()
	var rot := deg_to_rad(rot_deg)
	for i in range(n):
		var a := TAU * i / n
		out.append(c + Vector2(cos(a) * rx, sin(a) * ry).rotated(rot))
	return out

static func _arc(c: Vector2, r: float, a0: float, a1: float, n: int = 20) -> PackedVector2Array:
	var out := PackedVector2Array()
	for i in range(n + 1):
		var a := lerpf(a0, a1, float(i) / n)
		out.append(c + Vector2(cos(a), sin(a)) * r)
	return out

static func _join(parts: Array) -> PackedVector2Array:
	var out := PackedVector2Array()
	for p in parts:
		out.append_array(p)
	return out

static func _rev(pts: PackedVector2Array) -> PackedVector2Array:
	var out := PackedVector2Array()
	for i in range(pts.size() - 1, -1, -1):
		out.append(pts[i])
	return out

## Mirror across x = 50, keeping the polygon winding sensible (order reversed).
static func _mir(pts: PackedVector2Array) -> PackedVector2Array:
	var out := PackedVector2Array()
	for i in range(pts.size() - 1, -1, -1):
		out.append(Vector2(100.0 - pts[i].x, pts[i].y))
	return out

static func _poly(a: Array) -> PackedVector2Array:
	var out := PackedVector2Array()
	for p in a:
		out.append(Vector2(p[0], p[1]))
	return out

static func _rot(pts: PackedVector2Array, c: Vector2, deg: float) -> PackedVector2Array:
	var out := PackedVector2Array()
	for p in pts:
		out.append(c + (p - c).rotated(deg_to_rad(deg)))
	return out

## A curved tapering blade from `c` outward at angle `ang` (radians), length L, base half-width bw, curl sideways.
static func _blade(c: Vector2, ang: float, L: float, bw: float, curl: float) -> PackedVector2Array:
	var d := Vector2(cos(ang), sin(ang))
	var n := Vector2(-d.y, d.x)
	var tip := c + d * L + n * curl
	var left := _quad(c + n * bw, c + d * L * 0.55 + n * (curl * 1.1 + bw * 0.9), tip, 12)
	var right := _quad(tip, c + d * L * 0.5 + n * (curl * 0.2 - bw * 0.9), c - n * bw, 12)
	return _join([left, right])

# ---------------------------------------------------------------------------------------------
# The marks
# ---------------------------------------------------------------------------------------------
static func _build(key: String, small: bool) -> Array:
	var o: Array = []
	match key:
		"federation":
			# Starfleet delta: a fat swept arrowhead, concave trailing edge with a centre notch, a split line down it;
			# the full mark sits in its orbit ring.
			var t := Vector2(50, 0)
			var r_ := Vector2(97, 98)
			var l_ := Vector2(3, 98)
			var n_ := Vector2(50, 70)
			var delta := _join([_bez(t, Vector2(60, 34), Vector2(86, 62), r_), _bez(r_, Vector2(80, 88), Vector2(64, 80), n_),
				_bez(n_, Vector2(36, 80), Vector2(20, 88), l_), _bez(l_, Vector2(14, 62), Vector2(40, 34), t)])
			if small:
				o.append(["f", delta])
				o.append(["x", _bez(Vector2(50, 18), Vector2(42, 38), Vector2(44, 56), n_ + Vector2(0, -4)), 6.0])
			else:
				o.append(["S", _ellipse(Vector2(50, 72), 50, 14, -22.0, 48), 3.4])
				o.append(["h", delta, 7.0])
				o.append(["f", delta])
				o.append(["x", _bez(Vector2(50, 14), Vector2(43, 36), Vector2(45, 56), n_ + Vector2(0, -3)), 3.2])
		"klingon":
			# Trefoil: a tall central blade and two horns sweeping out and up from the same base point.
			var centre := _poly([[50, 0], [57, 30], [54, 62], [50, 100], [46, 62], [43, 30]])
			var lb := _join([_bez(Vector2(47, 80), Vector2(14, 94), Vector2(-4, 54), Vector2(6, 4)),
				_bez(Vector2(6, 4), Vector2(9, 42), Vector2(24, 62), Vector2(46, 66))])
			if small:
				centre = _poly([[50, 0], [60, 30], [57, 62], [50, 100], [43, 62], [40, 30]])
				lb = _join([_bez(Vector2(48, 84), Vector2(10, 98), Vector2(-8, 52), Vector2(4, 0)),
					_bez(Vector2(4, 0), Vector2(14, 40), Vector2(28, 60), Vector2(47, 62))])
			o.append(["f", centre])
			o.append(["f", lb])
			o.append(["f", _mir(lb)])
		"romulan":
			# Raptor with wings spread, head in profile, a sphere in each talon (full mark).
			var wing := _join([_bez(Vector2(46, 30), Vector2(30, 12), Vector2(14, 8), Vector2(2, 6)),
				_poly([[11, 24], [8, 27], [19, 38], [16, 41], [29, 50], [46, 52]])])
			if small:
				wing = _join([_bez(Vector2(46, 32), Vector2(30, 12), Vector2(14, 6), Vector2(0, 4)),
					_poly([[12, 28], [8, 31], [22, 46], [46, 56]])])
			o.append(["f", wing])
			o.append(["f", _rev(_mir(wing))])
			o.append(["f", _poly([[44, 22], [50, 14], [57, 18], [58, 26], [54, 62], [50, 70], [46, 62]])])
			o.append(["f", _poly([[44, 17], [34, 22], [43, 25]])]) # hooked beak
			o.append(["x", _circle(Vector2(52, 21), 1.8, 10), 2.0])
			if not small:
				o.append(["f", _poly([[43, 60], [36, 80], [50, 88], [64, 80], [57, 60]])])
				o.append(["x", _poly([[50, 66], [50, 84]]), 2.4])
				o.append(["f", _circle(Vector2(26, 86), 7.5, 20)])
				o.append(["f", _circle(Vector2(74, 86), 7.5, 20)])
				o.append(["f", _poly([[30, 78], [33, 70], [37, 76]])])
				o.append(["f", _poly([[70, 78], [67, 70], [63, 76]])])
			else:
				o.append(["f", _poly([[42, 56], [36, 84], [50, 92], [64, 84], [58, 56]])])
		"cardassian":
			# Union emblem: a heavy notched shield tapering to a point, split down the middle into mirrored regions, one dot off-centre.
			o.append(["f", _poly([[4, 4], [36, 4], [50, 20], [64, 4], [96, 4], [96, 32], [62, 96], [50, 100], [38, 96], [4, 32]])])
			o.append(["x", _poly([[50, 20], [50, 100]]), 3.5 if not small else 5.0])
			if not small:
				o.append(["x", _poly([[50, 56], [22, 34]]), 2.6])
				o.append(["x", _poly([[50, 56], [78, 34]]), 2.6])
				o.append(["x", _poly([[12, 16], [30, 16]]), 2.6])
				o.append(["x", _poly([[88, 16], [70, 16]]), 2.6])
				o.append(["c", _circle(Vector2(36, 44), 4.4, 12)])
			else:
				o.append(["c", _circle(Vector2(34, 36), 5.0, 10)])
		"ferengi":
			# Alliance emblem: a central disc held between two mirrored pincers, each cut into inner fields.
			var cc := Vector2(50, 50)
			var outer := _arc(cc, 47.0, deg_to_rad(108), deg_to_rad(252), 20)
			var inner := _arc(cc, 31.0 if not small else 32.0, deg_to_rad(252), deg_to_rad(108), 20)
			var pincer := _join([outer, inner])
			o.append(["f", pincer])
			o.append(["f", _mir(pincer)])
			o.append(["f", _circle(cc, 22.0, 24)])
			if not small:
				for a_ in [150.0, 205.0]:
					var d_ := Vector2(cos(deg_to_rad(a_)), sin(deg_to_rad(a_)))
					o.append(["x", _poly([[cc.x + d_.x * 32.0, cc.y + d_.y * 32.0], [cc.x + d_.x * 47.0, cc.y + d_.y * 47.0]]), 2.2])
					o.append(["x", _poly([[cc.x - d_.x * 32.0, cc.y + d_.y * 32.0], [cc.x - d_.x * 47.0, cc.y + d_.y * 47.0]]), 2.2])
		"dominion":
			# A rhomb holding a tall L (its long arm runs past the rhomb), a small cross and a short bar.
			var rh := _poly([[52, 16], [98, 58], [52, 100], [6, 58]])
			if small:
				o.append(["f", rh])
				o.append(["c", _poly([[50, 6], [60, 6], [60, 66], [78, 66], [78, 76], [50, 76]])])
				o.append(["x", _poly([[24, 52], [38, 52]]), 6.0])
			else:
				o.append(["S", rh, 4.0])
				o.append(["f", _poly([[52, 0], [62, 0], [62, 80], [84, 80], [84, 90], [52, 90]])])
				o.append(["s", _poly([[20, 52], [36, 52]]), 5.0])
				o.append(["s", _poly([[28, 44], [28, 60]]), 5.0])
				o.append(["s", _poly([[16, 70], [38, 70]]), 4.0])
		"borg":
			var hexa := _poly([[50, 4], [92, 27], [92, 73], [50, 96], [8, 73], [8, 27]])
			o.append(["f", hexa])
			o.append(["x", _poly([[8, 27], [50, 50], [92, 27]]), 3.2 if not small else 5.0])
			o.append(["x", _poly([[50, 50], [50, 96]]), 3.2 if not small else 5.0])
			if not small:
				o.append(["x", _poly([[18, 40], [30, 47], [30, 60]]), 2.0])
				o.append(["x", _poly([[82, 40], [70, 47], [70, 60]]), 2.0])
				o.append(["x", _poly([[36, 66], [36, 78], [46, 84]]), 2.0])
				o.append(["x", _poly([[64, 66], [64, 78], [54, 84]]), 2.0])
				o.append(["x", _poly([[50, 14], [50, 26]]), 2.0])
		"species8472":
			var c2 := Vector2(50, 52)
			o.append(["f", _blade(c2, deg_to_rad(-90), 48, 6.0, -8)])
			o.append(["f", _blade(c2, deg_to_rad(30), 48, 6.0, -8)])
			o.append(["f", _blade(c2, deg_to_rad(150), 48, 6.0, -8)])
			o.append(["f", _circle(c2, 12.0, 20)])
			o.append(["x", _circle(c2, 4.5, 12), 2.0])
		"breen":
			o.append(["f", _poly([[50, 2], [84, 12], [92, 52], [76, 92], [50, 98], [24, 92], [8, 52], [16, 12]])])
			o.append(["c", _poly([[24, 40], [76, 40], [76, 52], [24, 52]])])
			if not small:
				o.append(["x", _poly([[50, 58], [50, 88]]), 3.0])
		"hirogen":
			o.append(["s", _bez(Vector2(50, 90), Vector2(12, 80), Vector2(6, 40), Vector2(14, 6)), 11.0])
			o.append(["s", _bez(Vector2(50, 90), Vector2(88, 80), Vector2(94, 40), Vector2(86, 6)), 11.0])
			o.append(["s", _poly([[50, 90], [50, 28]]), 10.0])
			o.append(["s", _bez(Vector2(50, 54), Vector2(36, 50), Vector2(28, 40), Vector2(26, 26)), 7.0])
			o.append(["s", _bez(Vector2(50, 54), Vector2(64, 50), Vector2(72, 40), Vector2(74, 26)), 7.0])
		"kazon":
			var spikes := PackedVector2Array()
			for i in range(16):
				var a := TAU * i / 16.0 - PI / 2.0
				spikes.append(Vector2(50, 50) + Vector2(cos(a), sin(a)) * (46.0 if i % 2 == 0 else 28.0))
			o.append(["f", spikes])
			o.append(["c", _circle(Vector2(50, 50), 10.0, 16)])
		"xindi":
			var pent := PackedVector2Array()
			for i in range(5):
				var a2 := TAU * i / 5.0 - PI / 2.0
				pent.append(Vector2(50, 54) + Vector2(cos(a2), sin(a2)) * 46.0)
			o.append(["f", pent])
			o.append(["c", _poly([[50, 34], [64, 66], [36, 66]])])
		"krenim":
			o.append(["S", _circle(Vector2(50, 50), 40.0, 36), 9.0 if not small else 12.0])
			o.append(["s", _poly([[50, 50], [50, 22]]), 8.0])
			o.append(["s", _poly([[50, 50], [70, 62]]), 8.0])
			o.append(["f", _circle(Vector2(50, 50), 7.0, 14)])
		"tholian":
			var hx := PackedVector2Array()
			for i in range(6):
				var a3 := TAU * i / 6.0
				hx.append(Vector2(50, 50) + Vector2(cos(a3), sin(a3)) * 47.0)
			o.append(["f", hx])
			var hx2 := PackedVector2Array()
			for i in range(6):
				var a4 := TAU * i / 6.0 + PI / 6.0
				hx2.append(Vector2(50, 50) + Vector2(cos(a4), sin(a4)) * 22.0)
			o.append(["c", hx2])
		"gorn":
			o.append(["f", _ellipse(Vector2(50, 50), 46, 30, 0.0, 36)])
			o.append(["c", _poly([[50, 24], [58, 50], [50, 76], [42, 50]])])
		"orion":
			o.append(["f", _poly([[50, 4], [96, 90], [4, 90]])])
			o.append(["c", _circle(Vector2(50, 62), 12.0, 16)])
			o.append(["f", _circle(Vector2(50, 62), 5.0, 10)])
		"maquis":
			o.append(["S", _circle(Vector2(50, 50), 42.0, 36), 8.0 if not small else 11.0])
			o.append(["s", _poly([[26, 26], [74, 74]]), 11.0])
			o.append(["s", _poly([[74, 26], [26, 74]]), 11.0])
		"devore":
			o.append(["f", _poly([[30, 4], [70, 4], [96, 30], [96, 70], [70, 96], [30, 96], [4, 70], [4, 30]])])
			o.append(["c", _poly([[26, 46], [74, 46], [74, 56], [26, 56]])])
			o.append(["c", _poly([[46, 22], [56, 22], [56, 78], [46, 78]])])
		"minor":
			o.append(["S", _circle(Vector2(50, 50), 41.0, 36), 7.0 if not small else 10.0])
			o.append(["f", _poly([[50, 18], [57, 43], [82, 50], [57, 57], [50, 82], [43, 57], [18, 50], [43, 43]])])
		_:
			# Neutral / unknown contact: a diamond with a question-mark notch.
			o.append(["f", _poly([[50, 3], [97, 50], [50, 97], [3, 50]])])
			var q := _join([_bez(Vector2(36, 38), Vector2(36, 18), Vector2(66, 18), Vector2(64, 38)),
				_bez(Vector2(64, 38), Vector2(63, 52), Vector2(50, 50), Vector2(50, 63))])
			o.append(["x", q, 7.0 if small else 5.5])
			o.append(["c", _circle(Vector2(50, 75), 4.6 if small else 4.0, 12)])
	return o
