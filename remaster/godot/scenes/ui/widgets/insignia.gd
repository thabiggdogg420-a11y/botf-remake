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
const ALL := ["federation", "ufp", "klingon", "romulan", "cardassian", "ferengi", "dominion", "borg", "species8472", "breen", "hirogen",
	"kazon", "xindi", "krenim", "tholian", "gorn", "orion", "maquis", "devore", "minor", "neutral"]
const ALIASES := {"fed": "federation", "starfleet": "federation", "klingons": "klingon", "romulans": "romulan",
	"reman": "romulan", "cardassians": "cardassian", "ferengis": "ferengi", "jemhadar": "dominion", "8472": "species8472",
	"species 8472": "species8472", "unknown": "neutral", "contact": "neutral"}
const SMALL_PX := 36.0
## Marks whose full variant stays legible lower down switch to the small variant later.
const SMALL_BELOW := {"federation": 26.0, "ufp": 40.0}

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
	if style == "seal" and key == "federation":
		key = "ufp"
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
			# TNG/DS9 combadge: the Starfleet delta (swept arrowhead, concave trailing edge, centre crease) laid over a
			# horizontal oval. The oval is filled; the delta is cut free of it by a halo.
			var t := Vector2(50, 0)
			var r_ := Vector2(74, 88)
			var l_ := Vector2(26, 88)
			var n_ := Vector2(50, 70)
			var delta := _join([_bez(t, Vector2(56, 30), Vector2(70, 60), r_), _bez(r_, Vector2(70, 84), Vector2(60, 76), n_),
				_bez(n_, Vector2(40, 76), Vector2(30, 84), l_), _bez(l_, Vector2(30, 60), Vector2(44, 30), t)])
			if small:
				o.append(["f", _ellipse(Vector2(50, 72), 50, 22, 0.0, 40)])
				o.append(["h", delta, 11.0])
				o.append(["f", delta])
			else:
				o.append(["f", _ellipse(Vector2(50, 72), 50, 22, 0.0, 48)])
				o.append(["h", delta, 9.0])
				o.append(["f", delta])
				o.append(["x", _bez(Vector2(50, 16), Vector2(47, 36), Vector2(49, 56), n_ + Vector2(0, -3)), 2.8])
		"ufp":
			# United Federation of Planets seal: a starfield disc inside a ring, held in two laurel branches.
			o.append(["S", _circle(Vector2(50, 50), 32.0, 40), 3.2])
			o.append(["f", _circle(Vector2(50, 50), 27.0, 36)])
			for s_ in [[40, 40, 4.4], [62, 44, 5.4], [50, 62, 4.6], [30, 56, 2.6], [70, 62, 2.4], [48, 30, 2.4]]:
				var sp := Vector2(s_[0], s_[1])
				var rr: float = s_[2]
				o.append(["c", _poly([[sp.x, sp.y - rr], [sp.x + rr * 0.28, sp.y - rr * 0.28], [sp.x + rr, sp.y], [sp.x + rr * 0.28, sp.y + rr * 0.28],
					[sp.x, sp.y + rr], [sp.x - rr * 0.28, sp.y + rr * 0.28], [sp.x - rr, sp.y], [sp.x - rr * 0.28, sp.y - rr * 0.28]])])
			var leaves := 8 if not small else 6
			for side in [-1.0, 1.0]:
				for i in range(leaves):
					var a_: float = deg_to_rad(lerpf(-50.0, 62.0, float(i) / float(leaves - 1)))
					var cx_: float = 50.0 + side * cos(a_) * 43.0
					var cy_: float = 50.0 + sin(a_) * 43.0
					var tang: float = a_ + PI / 2.0
					var rot_deg: float = rad_to_deg(tang) if side > 0 else rad_to_deg(PI - tang)
					o.append(["f", _ellipse(Vector2(cx_, cy_), 8.0, 3.6, rot_deg + (-22.0 * side), 14)])
		"klingon":
			# Imperial trefoil (traced from the Empire's emblem): a tall spear-blade whose foot is an inverted V, and two sickle
			# blades sweeping out and down beneath it, each cut free of the spear by a thin gap.
			var spear := _join([_bez(Vector2(50, 0), Vector2(52, 30), Vector2(58, 52), Vector2(63, 64)), _poly([[62, 70], [50, 88], [38, 70]]),
				_bez(Vector2(37, 64), Vector2(42, 52), Vector2(48, 30), Vector2(50, 0))])
			var lb := _join([_bez(Vector2(47, 90), Vector2(32, 93), Vector2(14, 97), Vector2(2, 100)), _bez(Vector2(2, 100), Vector2(10, 91), Vector2(24, 82), Vector2(37, 75)),
				_poly([[48, 86]])])
			if small:
				spear = _join([_bez(Vector2(50, 0), Vector2(54, 30), Vector2(63, 52), Vector2(67, 62)), _poly([[64, 72], [50, 90], [36, 72]]),
					_bez(Vector2(33, 62), Vector2(37, 52), Vector2(46, 30), Vector2(50, 0))])
				lb = _join([_bez(Vector2(46, 90), Vector2(30, 94), Vector2(10, 98), Vector2(0, 100)), _bez(Vector2(0, 100), Vector2(10, 90), Vector2(24, 80), Vector2(35, 73)),
					_poly([[48, 86]])])
			o.append(["f", lb])
			o.append(["f", _mir(lb)])
			o.append(["h", spear, 4.2 if not small else 5.0])
			o.append(["f", spear])
		"romulan":
			# Imperial raptor: wings spread in layered feathers, hooked head, a long tail point and the twin worlds in its talons.
			var wing := _join([_bez(Vector2(46, 34), Vector2(30, 14), Vector2(14, 8), Vector2(1, 8)),
				_poly([[10, 22], [4, 24], [16, 34], [9, 38], [22, 46], [15, 52], [32, 58], [46, 56]])])
			if small:
				wing = _join([_bez(Vector2(46, 34), Vector2(30, 12), Vector2(14, 6), Vector2(0, 6)),
					_poly([[10, 26], [4, 30], [22, 46], [14, 54], [46, 60]])])
			o.append(["f", wing])
			o.append(["f", _rev(_mir(wing))])
			o.append(["f", _poly([[44, 24], [50, 14], [57, 18], [58, 28], [55, 58], [50, 100], [45, 58]])])
			o.append(["f", _poly([[44, 18], [33, 22], [43, 27]])]) # hooked beak
			o.append(["x", _circle(Vector2(52, 22), 1.8, 10), 2.0])
			if not small:
				o.append(["f", _circle(Vector2(31, 74), 8.0, 20)])
				o.append(["f", _circle(Vector2(69, 74), 8.0, 20)])
				o.append(["f", _poly([[34, 66], [38, 58], [43, 64]])])
				o.append(["f", _poly([[66, 66], [62, 58], [57, 64]])])
			else:
				o.append(["f", _circle(Vector2(30, 76), 9.0, 16)])
				o.append(["f", _circle(Vector2(70, 76), 9.0, 16)])
		"cardassian":
			# Union emblem (traced from the Union's crest): a hooded crest with swept shoulders, a stem below them, an arrow notch
			# in the hood and an elongated cartouche down the spine.
			var half := _join([_bez(Vector2(50, 0), Vector2(60, 0), Vector2(66, 6), Vector2(68, 13)), _bez(Vector2(68, 13), Vector2(82, 14), Vector2(94, 26), Vector2(99, 42)),
				_bez(Vector2(99, 42), Vector2(84, 44), Vector2(70, 48), Vector2(62, 58)), _poly([[61, 64], [63, 74], [68, 84], [62, 94], [56, 100]])])
			var left := _mir(half)
			left.remove_at(left.size() - 1)
			var crest := _join([half, left])
			o.append(["f", crest])
			if small:
				o.append(["c", _poly([[50, 18], [57, 36], [57, 68], [50, 88], [43, 68], [43, 36]])])
			else:
				o.append(["c", _poly([[40, 6], [60, 6], [52, 15], [50, 24], [48, 15]])])
				o.append(["X", _poly([[50, 28], [57, 40], [57, 68], [50, 90], [43, 68], [43, 40]]), 2.2])
				o.append(["c", _poly([[50, 40], [53, 48], [52, 66], [50, 78], [48, 66], [47, 48]])])
		"ferengi":
			# Alliance seal: a bullet-shaped plate, rounded at the left and chamfered at the right, a bar across it and the
			# eye ring set in the right half.
			var plate := _join([_poly([[30, 6], [70, 6], [100, 34], [100, 66], [70, 94], [30, 94]]),
				_bez(Vector2(30, 94), Vector2(8, 92), Vector2(0, 72), Vector2(0, 50)), _bez(Vector2(0, 50), Vector2(0, 28), Vector2(8, 8), Vector2(30, 6))])
			o.append(["f", plate])
			if small:
				o.append(["x", _poly([[0, 50], [52, 50]]), 4.0])
				o.append(["x", _poly([[56, 50], [72, 34], [90, 50], [72, 66]]), 4.5])
				o.append(["c", _circle(Vector2(72, 50), 5.0, 12)])
			else:
				o.append(["x", _poly([[0, 50], [46, 50]]), 3.2])
				o.append(["X", _poly([[50, 50], [72, 30], [94, 50], [72, 70]]), 3.0])
				o.append(["X", _circle(Vector2(72, 50), 9.0, 18), 2.8])
				o.append(["c", _circle(Vector2(72, 50), 3.6, 10)])
		"dominion":
			# A framed rhomb carrying the Founders' hook: a tall stem rising past the frame, curling to a hook at the foot,
			# a four-point star and a short bar to the left of it.
			var rh := _poly([[50, 14], [98, 58], [50, 100], [2, 58]])
			var hook := _join([_poly([[58, 0], [64, 0], [64, 66]]), _bez(Vector2(64, 66), Vector2(64, 82), Vector2(52, 88), Vector2(40, 82)),
				_bez(Vector2(40, 82), Vector2(52, 82), Vector2(58, 76), Vector2(58, 64))])
			if small:
				o.append(["S", rh, 8.0])
				o.append(["f", _poly([[56, 0], [68, 0], [68, 64]])])
				o.append(["f", hook])
				o.append(["f", _poly([[34, 48], [40, 56], [34, 64], [28, 56]])])
			else:
				o.append(["S", rh, 3.4])
				o.append(["f", hook])
				o.append(["f", _poly([[34, 44], [37, 53], [46, 56], [37, 59], [34, 68], [31, 59], [22, 56], [31, 53]])])
				o.append(["s", _poly([[12, 56], [20, 56]]), 2.6])
		"borg":
			# Collective glyph: a tall ovoid, three slits in its crown, nested arches below.
			o.append(["f", _ellipse(Vector2(50, 50), 31, 50, 0.0, 48)])
			if small:
				o.append(["x", _poly([[50, 10], [50, 44]]), 6.0])
				o.append(["x", _join([_bez(Vector2(36, 92), Vector2(34, 60), Vector2(44, 52), Vector2(50, 52)), _bez(Vector2(50, 52), Vector2(56, 52), Vector2(66, 60), Vector2(64, 92))]), 5.0])
			else:
				for xs in [36.0, 50.0, 64.0]:
					o.append(["x", _bez(Vector2(xs, 46), Vector2(xs - 1.5, 30), Vector2(xs - 1.5, 18), Vector2(xs, 8)), 3.2])
				o.append(["x", _join([_bez(Vector2(32, 92), Vector2(28, 62), Vector2(42, 54), Vector2(50, 54)), _bez(Vector2(50, 54), Vector2(58, 54), Vector2(72, 62), Vector2(68, 92))]), 3.4])
				o.append(["x", _join([_bez(Vector2(41, 92), Vector2(40, 72), Vector2(46, 66), Vector2(50, 66)), _bez(Vector2(50, 66), Vector2(54, 66), Vector2(60, 72), Vector2(59, 92))]), 3.0])
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
