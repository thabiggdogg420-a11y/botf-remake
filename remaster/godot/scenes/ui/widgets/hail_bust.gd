class_name HailBust
extends RefCounted
## Leader busts for the hail viewscreen, drawn in-house as vector silhouettes (Simon, 10 Nov 2026 Trek pass call 2:
## "leader portraits are drawn silhouette busts on a viewscreen, no real likenesses"). No image file, nothing traced.
##
##   HailBust.draw(canvas_item, key, rect, rim_colour)
##
## key: "fed", "kli", "rom", "car", "fer", "dom", "borg", "breen", "terran", "neutral", or a mirror twin "mirror.<key>"
##   (the Mirror Universe pack: the same species in the Alliance's harness and sash). Every bust is one dark figure with a
##   rim light in the empire's colour, in a 0..100 box (head top near y 8, shoulders run off the bottom edge).
## key_for(empire, mirror) maps a power id (1..8) to its key; under the Mirror pack the Federation becomes the Terran Empire.

const KEYS := ["fed", "kli", "rom", "car", "fer", "dom", "borg", "breen", "terran"]
const BY_ID := {1: "fed", 2: "kli", 3: "rom", 4: "car", 5: "fer", 6: "dom", 7: "borg", 8: "breen"}
const FIGURE := Color(0.02, 0.025, 0.04, 1.0)

static var _cache := {}

static func key_for(empire: int, mirror: bool = false) -> String:
	var k: String = BY_ID.get(empire, "neutral")
	if mirror and k != "neutral":
		return "terran" if k == "fed" else "mirror." + k
	return k

## True when the running game carries a pack that renames the powers (the Mirror Universe, schema 25).
static func mirror_active() -> bool:
	return not Lcars._pack_names.is_empty()

static func draw(ci: CanvasItem, key: String, rect: Rect2, rim: Color) -> void:
	var px := minf(rect.size.x, rect.size.y)
	if px < 4.0:
		return
	var o := rect.position + Vector2((rect.size.x - px) * 0.5, rect.size.y - px)
	var k := px / 100.0
	var parts := shapes(key)
	# rim light: each shape drawn a touch larger in the empire's colour, then the figure over it
	for s in parts["body"]:
		_fill(ci, _place(_grow(s, 1.6), o, k), Color(rim, 0.85))
	for s in parts["body"]:
		_fill(ci, _place(s, o, k), FIGURE)
	for l in parts["lines"]:
		ci.draw_polyline(_place(l, o, k), Color(rim, 0.55), maxf(1.0, 1.1 * k), true)
	for e in parts["glow"]:
		var c: Vector2 = o + (e[0] as Vector2) * k
		ci.draw_circle(c, float(e[1]) * k, Color(e[2] as Color))

## {body: [polygons], lines: [polylines], glow: [[centre, radius, colour]]} in the 0..100 box.
static func shapes(key: String) -> Dictionary:
	if not _cache.has(key):
		_cache[key] = _build(key)
	return _cache[key]

static func _build(key: String) -> Dictionary:
	var mirror := key.begins_with("mirror.")
	var race := key.trim_prefix("mirror.")
	var body: Array = []
	var lines: Array = []
	var glow: Array = []
	var head_c := Vector2(50, 34)
	var head_rx := 15.0
	var head_ry := 19.0
	var shoulder_w := 46.0
	var shoulder_y := 74.0
	match race:
		"kli":
			head_rx = 16.0
			shoulder_w = 50.0
			# long hair falling to the shoulders behind the head
			# long hair in two heavy locks falling past the shoulders, and a beard
			body.append(_poly([[36, 26], [30, 36], [25, 50], [24, 66], [33, 70], [37, 52], [39, 36]]))
			body.append(_poly([[64, 26], [70, 36], [75, 50], [76, 66], [67, 70], [63, 52], [61, 36]]))
			body.append(_poly([[41, 46], [44, 57], [50, 61], [56, 57], [59, 46], [50, 50]]))
			# the forehead ridges, drawn on a round head (a crest above it read as a hood)
			lines.append(_poly([[39, 24], [44, 21], [50, 23], [56, 21], [61, 24]]))
			lines.append(_poly([[42, 19], [46, 17], [50, 18.5], [54, 17], [58, 19]]))
			lines.append(_poly([[50, 14], [50, 23]]))
			# armour plates and baldric
			lines.append(_poly([[26, 84], [74, 96]]))
			lines.append(_poly([[18, 80], [32, 76], [36, 88]]))
			lines.append(_poly([[82, 80], [68, 76], [64, 88]]))
		"rom":
			head_rx = 14.0
			shoulder_w = 52.0
			# bowl haircut: flat top, square fringe
			body.append(_poly([[35, 30], [35, 20], [39, 15], [50, 13.5], [61, 15], [65, 20], [65, 30]]))
			# pointed ears
			body.append(_poly([[36, 33], [30, 22], [37, 38]]))
			body.append(_poly([[64, 33], [70, 22], [63, 38]]))
			lines.append(_poly([[37, 26], [63, 26]]))
			# high padded shoulders (wider and raised)
			shoulder_y = 70.0
			lines.append(_poly([[22, 78], [50, 90], [78, 78]]))
		"car":
			head_rx = 13.5
			head_ry = 20.0
			# spoon forehead and the neck ridges flaring to the shoulders
			body.append(_ellipse(Vector2(50, 21), 4.5, 7.0, 0.0))
			body.append(_poly([[36, 40], [26, 62], [34, 66], [44, 50]]))
			body.append(_poly([[64, 40], [74, 62], [66, 66], [56, 50]]))
			lines.append(_poly([[50, 15], [50, 28]]))
			lines.append(_poly([[30, 60], [40, 46]]))
			lines.append(_poly([[70, 60], [60, 46]]))
			# segmented armour collar
			for i in 4:
				lines.append(_poly([[20 + i * 4, 82 - i * 3], [80 - i * 4, 82 - i * 3]]).duplicate())
			shoulder_w = 50.0
		"fer":
			head_rx = 16.5
			head_ry = 17.0
			head_c = Vector2(50, 36)
			# the great ears
			body.append(_ellipse(Vector2(28, 32), 9.5, 13.0, -15.0))
			body.append(_ellipse(Vector2(72, 32), 9.5, 13.0, 15.0))
			lines.append(_arc(Vector2(28, 32), 5.5, 100.0, 300.0))
			lines.append(_arc(Vector2(72, 32), 5.5, -120.0, 80.0))
			lines.append(_poly([[44, 26], [50, 23], [56, 26]]))
			shoulder_w = 42.0
		"dom":
			# a Vorta: the ear ridge that climbs to the crown
			head_rx = 13.5
			head_ry = 19.5
			body.append(_poly([[36, 40], [33, 26], [38, 16], [41, 30], [39, 42]]))
			body.append(_poly([[64, 40], [67, 26], [62, 16], [59, 30], [61, 42]]))
			lines.append(_poly([[37, 38], [35, 28], [38, 20]]))
			lines.append(_poly([[63, 38], [65, 28], [62, 20]]))
			# high Vorta collar
			body.append(_poly([[34, 58], [40, 48], [50, 56], [60, 48], [66, 58], [50, 70]]))
			lines.append(_poly([[28, 84], [50, 72], [72, 84]]))
		"borg":
			# the Queen: bald dome, tubes down the neck into the shoulders, an implant at one eye
			head_rx = 14.0
			head_ry = 19.0
			body.append(_poly([[39, 44], [32, 58], [30, 72], [35, 72], [38, 58], [43, 48]]))
			body.append(_poly([[61, 44], [68, 58], [70, 72], [65, 72], [62, 58], [57, 48]]))
			lines.append(_poly([[36, 52], [33, 70]]))
			lines.append(_poly([[64, 52], [67, 70]]))
			lines.append(_poly([[42, 18], [50, 15], [58, 18]]))
			lines.append(_poly([[40, 24], [45, 22]]))
			lines.append(_poly([[24, 86], [36, 80], [50, 84], [64, 80], [76, 86]]))
			glow.append([Vector2(57, 33), 2.4, Color(0.35, 1.0, 0.45, 0.95)])
			glow.append([Vector2(57, 33), 4.5, Color(0.35, 1.0, 0.45, 0.25)])
		"breen":
			# refrigeration helmet: domed, a long snout visor, ribbed
			head_rx = 16.0
			head_ry = 18.0
			body.append(_poly([[40, 34], [30, 44], [32, 50], [50, 52], [68, 50], [70, 44], [60, 34]]))
			lines.append(_poly([[33, 46], [50, 49], [67, 46]]))
			lines.append(_poly([[38, 40], [50, 36], [62, 40]]))
			for i in 3:
				lines.append(_poly([[37 + i * 2, 22 + i * 4], [63 - i * 2, 22 + i * 4]]).duplicate())
			glow.append([Vector2(50, 42), 2.0, Color(0.6, 0.95, 0.75, 0.9)])
			shoulder_w = 50.0
			lines.append(_poly([[20, 84], [80, 84]]))
		"terran":
			# the Terran Empire: a human officer, swept-back hair, goatee, the Imperial sash across the chest
			head_rx = 14.5
			body.append(_poly([[36, 28], [38, 18], [46, 13], [56, 13.5], [63, 18], [65, 28], [62, 22], [50, 18], [39, 22]]))
			body.append(_poly([[45, 50], [50, 56], [55, 50], [50, 52]]))
			lines.append(_poly([[24, 74], [72, 100]]))
			lines.append(_poly([[30, 72], [76, 98]]))
			lines.append(_poly([[40, 64], [50, 70], [60, 64]]))
		"neutral":
			pass
		_:  # fed
			# a Starfleet officer: neat hair, the uniform's shoulder yoke
			head_rx = 14.5
			body.append(_poly([[36, 28], [37, 19], [44, 14], [56, 14], [63, 19], [64, 28], [60, 21], [50, 19], [40, 21]]))
			lines.append(_poly([[18, 84], [36, 76], [50, 80], [64, 76], [82, 84]]))
			lines.append(_poly([[40, 64], [50, 70], [60, 64]]))
	# head, neck and shoulders (shared)
	body.append(_ellipse(head_c, head_rx, head_ry, 0.0))
	body.append(_poly([[50 - head_rx * 0.55, head_c.y + head_ry * 0.6], [50 - head_rx * 0.6, 64], [50 + head_rx * 0.6, 64], [50 + head_rx * 0.55, head_c.y + head_ry * 0.6]]))
	var sw := shoulder_w
	var shoulders := _join([
		[Vector2(50 - sw, 101), Vector2(50 - sw, shoulder_y + 14)],
		_bez(Vector2(50 - sw, shoulder_y + 14), Vector2(50 - sw, shoulder_y - 2), Vector2(50 - sw * 0.55, shoulder_y - 8), Vector2(50 - head_rx * 0.6, 62)),
		_bez(Vector2(50 + head_rx * 0.6, 62), Vector2(50 + sw * 0.55, shoulder_y - 8), Vector2(50 + sw, shoulder_y - 2), Vector2(50 + sw, shoulder_y + 14)),
		[Vector2(50 + sw, 101)],
	])
	body.append(shoulders)
	if mirror:
		# the Alliance's harness: a broad sash and a pauldron strap
		lines.append(_poly([[50 - sw * 0.8, shoulder_y + 2], [50 + sw * 0.6, 100]]))
		lines.append(_poly([[50 - sw * 0.8 + 6, shoulder_y + 0], [50 + sw * 0.6 + 6, 98]]))
		lines.append(_arc(Vector2(50 + sw * 0.5, shoulder_y + 8), 7.0, 180.0, 360.0))
	return {"body": body, "lines": lines, "glow": glow}

# ---------------------------------------------------------------------------------------------
static func _fill(ci: CanvasItem, pts: PackedVector2Array, col: Color) -> void:
	if pts.size() < 3:
		return
	if not Geometry2D.triangulate_polygon(pts).is_empty():
		ci.draw_colored_polygon(pts, col)
	var loop := pts.duplicate()
	loop.append(pts[0])
	ci.draw_polyline(loop, col, 1.0, true)

static func _place(pts: PackedVector2Array, o: Vector2, k: float) -> PackedVector2Array:
	var out := PackedVector2Array()
	for p in pts:
		out.append(o + p * k)
	return out

## Grows a polygon by `d` units away from its centroid (the rim light).
static func _grow(pts: PackedVector2Array, d: float) -> PackedVector2Array:
	var c := Vector2.ZERO
	for p in pts:
		c += p
	c /= maxf(1.0, pts.size())
	var out := PackedVector2Array()
	for p in pts:
		var v := p - c
		out.append(p + (v.normalized() * d if v.length() > 0.01 else Vector2.ZERO))
	return out

static func _poly(a: Array) -> PackedVector2Array:
	var out := PackedVector2Array()
	for p in a:
		out.append(Vector2(float(p[0]), float(p[1])))
	return out

static func _ellipse(c: Vector2, rx: float, ry: float, rot_deg: float, n: int = 40) -> PackedVector2Array:
	var out := PackedVector2Array()
	var r := deg_to_rad(rot_deg)
	for i in n:
		var a := TAU * i / n
		out.append(c + Vector2(cos(a) * rx, sin(a) * ry).rotated(r))
	return out

static func _arc(c: Vector2, r: float, a0: float, a1: float, n: int = 16) -> PackedVector2Array:
	var out := PackedVector2Array()
	for i in n + 1:
		var a := deg_to_rad(lerpf(a0, a1, float(i) / n))
		out.append(c + Vector2(cos(a), sin(a)) * r)
	return out

static func _bez(a: Vector2, b: Vector2, c: Vector2, d: Vector2, n: int = 14) -> PackedVector2Array:
	var out := PackedVector2Array()
	for i in n + 1:
		var t := float(i) / n
		var u := 1.0 - t
		out.append(a * u * u * u + b * 3.0 * u * u * t + c * 3.0 * u * t * t + d * t * t * t)
	return out

static func _join(parts: Array) -> PackedVector2Array:
	var out := PackedVector2Array()
	for p in parts:
		for v in p:
			if out.is_empty() or out[out.size() - 1].distance_to(v) > 0.01:
				out.append(v)
	return out
