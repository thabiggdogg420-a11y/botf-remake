class_name ShipGlyph
extends RefCounted
## In-house vector pictures of hulls, starbases and sensor contacts. No image files, nothing copied from anywhere.
## A hull is a top-down silhouette per hull FAMILY (saucer and nacelles, bird of prey, warbird, Galor spade, ...), bow
## towards +X, drawn in a unit box (x -0.5..0.5, y -0.5..0.5). draw_hull rotates and scales it into any CanvasItem.
##
##   ShipGlyph.draw_hull(ci, family, centre, length_px, angle_rad, colour, detail = true, outline = true)
##   ShipGlyph.family_for(owner, class_name) -> family
##   ShipGlyph.draw_station(ci, centre, radius, colour, starbase = true)
##   ShipGlyph.draw_contact(ci, centre, radius, colour, owner, class_name)
##
## The baked ship icons (ShipIcons / ShipPicture) are the first choice in menus; this is the fallback and the map glyph.

const FAMILIES := ["saucer", "fed_dart", "kli_bop", "kli_cruiser", "warbird", "galor", "marauder", "jemhadar", "borg_cube", "bioship", "raider"]

## Federation classes that are compact wedges, not saucers (from the roster class names).
const FED_DARTS := ["defiant", "saber", "steamrunner", "akira", "norway", "sydney", "peregrine", "raven", "yeager", "nova"]
const KLI_BOPS := ["bop", "bird", "b'rel", "brel", "k'vort", "kvort", "negh'var_bop"]
const KEYS := ["", "federation", "klingon", "romulan", "cardassian", "ferengi"]

static var _cache := {}

## Roster-style class name of a fleet/contact/ship Dictionary ("Oberth", "K'T'Inga"), "" when there is none.
static func class_of(ship: Dictionary) -> String:
	var dc := str(ship.get("designClass", ""))
	if dc != "":
		return dc
	var nm := str(ship.get("name", ship.get("className", "")))
	for sep in [" — ", " - ", " â€” "]:
		if nm.find(sep) > 0:
			return nm.substr(0, nm.find(sep)).strip_edges()
	# mojibake em dash from an old fixture: cut at the first non-ASCII character
	for i in nm.length():
		if nm.unicode_at(i) > 127:
			return nm.substr(0, i).strip_edges()
	return nm.strip_edges()

static func family_for(owner: Variant, klass: String = "") -> String:
	var k := klass.to_lower()
	var o: int = int(owner) if (owner is int or owner is float) else _owner_from_key(str(owner))
	if k.find("cube") >= 0 or k.find("borg") >= 0 or k.find("sphere") >= 0:
		return "borg_cube"
	if k.find("jem") >= 0 or k.find("dominion") >= 0 or k.find("vorta") >= 0 or k.find("cardassian-dominion") >= 0:
		return "jemhadar"
	if k.find("8472") >= 0 or k.find("bioship") >= 0:
		return "bioship"
	match o:
		1:
			for d in FED_DARTS:
				if k == d or k.begins_with(d + " "):
					return "fed_dart"
			return "saucer"
		2:
			return "kli_bop" if _is_bop(k) else "kli_cruiser"
		3:
			return "kli_bop" if _is_bop(k) else "warbird"
		4:
			return "galor"
		5:
			return "marauder"
	return "raider"

static func _is_bop(k: String) -> bool:
	for b in KLI_BOPS:
		if k.find(b) >= 0:
			return true
	return false

static func _owner_from_key(s: String) -> int:
	return maxi(0, KEYS.find(s.to_lower()))

# ---------------------------------------------------------------------------------------------
# Silhouette tables
# ---------------------------------------------------------------------------------------------
static func _p(a: Array) -> PackedVector2Array:
	var out := PackedVector2Array()
	for q in a:
		out.append(Vector2(q[0], q[1]))
	return out

static func _circ(c: Vector2, rx: float, ry: float, n: int = 28) -> PackedVector2Array:
	var out := PackedVector2Array()
	for i in range(n):
		var a := TAU * i / n
		out.append(c + Vector2(cos(a) * rx, sin(a) * ry))
	return out

static func _flip(pts: PackedVector2Array) -> PackedVector2Array:
	var out := PackedVector2Array()
	for i in range(pts.size() - 1, -1, -1):
		out.append(Vector2(pts[i].x, -pts[i].y))
	return out

static func _arc(c: Vector2, r: float, a0: float, a1: float, n: int = 18) -> PackedVector2Array:
	var out := PackedVector2Array()
	for i in range(n + 1):
		var a := lerpf(a0, a1, float(i) / n)
		out.append(c + Vector2(cos(a), sin(a)) * r)
	return out

## Returns {"polys": [PackedVector2Array...], "lines": [PackedVector2Array...], "dots": [[Vector2, r]...]}.
static func shape(family: String) -> Dictionary:
	if _cache.has(family):
		return _cache[family]
	var polys: Array = []
	var lines: Array = []
	var dots: Array = []
	match family:
		"saucer":
			# Primary saucer, neck, secondary hull, twin nacelles on pylons.
			polys.append(_circ(Vector2(0.22, 0), 0.27, 0.27))
			polys.append(_p([[0.05, -0.05], [-0.14, -0.06], [-0.14, 0.06], [0.05, 0.05]]))
			polys.append(_p([[-0.1, -0.075], [-0.36, -0.065], [-0.42, 0], [-0.36, 0.065], [-0.1, 0.075]]))
			var nac := _p([[0.0, 0.15], [-0.44, 0.15], [-0.5, 0.18], [-0.44, 0.21], [0.0, 0.21], [0.04, 0.18]])
			polys.append(nac)
			polys.append(_flip(nac))
			lines.append(_p([[-0.3, 0.065], [-0.27, 0.15]]))
			lines.append(_p([[-0.3, -0.065], [-0.27, -0.15]]))
			lines.append(PackedVector2Array(_arc(Vector2(0.22, 0), 0.16, 0, TAU, 20)))
			dots.append([Vector2(0.22, 0), 0.04])
		"fed_dart":
			polys.append(_p([[0.5, 0], [0.2, -0.1], [-0.05, -0.2], [-0.4, -0.2], [-0.46, -0.1], [-0.46, 0.1], [-0.4, 0.2], [-0.05, 0.2], [0.2, 0.1]]))
			var nac2 := _p([[-0.05, 0.2], [-0.5, 0.24], [-0.5, 0.3], [-0.05, 0.28]])
			polys.append(nac2)
			polys.append(_flip(nac2))
			lines.append(_p([[0.35, 0], [-0.3, 0]]))
			dots.append([Vector2(0.18, 0), 0.04])
		"kli_bop":
			# Swept wings with a long neck and a small head.
			polys.append(_p([[0.5, 0], [0.42, -0.05], [0.14, -0.05], [-0.02, -0.3], [-0.3, -0.5], [-0.4, -0.5], [-0.38, -0.12], [-0.32, 0], [-0.38, 0.12], [-0.4, 0.5], [-0.3, 0.5], [-0.02, 0.3], [0.14, 0.05], [0.42, 0.05]]))
			lines.append(_p([[0.4, 0], [-0.25, 0]]))
			lines.append(_p([[0.1, -0.06], [-0.3, -0.44]]))
			lines.append(_p([[0.1, 0.06], [-0.3, 0.44]]))
			dots.append([Vector2(0.4, 0), 0.04])
		"kli_cruiser":
			# Bulbous head on a boom, broad swept body, engine pods.
			polys.append(_circ(Vector2(0.38, 0), 0.12, 0.09))
			polys.append(_p([[0.3, -0.03], [0.0, -0.03], [0.0, 0.03], [0.3, 0.03]]))
			polys.append(_p([[0.04, -0.03], [-0.2, -0.3], [-0.46, -0.24], [-0.46, 0.24], [-0.2, 0.3], [0.04, 0.03]]))
			var pod := _p([[-0.2, 0.3], [-0.5, 0.3], [-0.5, 0.38], [-0.2, 0.38]])
			polys.append(pod)
			polys.append(_flip(pod))
			lines.append(_p([[-0.46, 0], [0.0, 0]]))
		"warbird":
			# A wide crescent with the command spine across its heart.
			var outer := _arc(Vector2(0.0, 0), 0.47, deg_to_rad(38), deg_to_rad(322), 26)
			var inner := _arc(Vector2(0.1, 0), 0.34, deg_to_rad(322), deg_to_rad(38), 26)
			polys.append(PackedVector2Array(outer) + PackedVector2Array(inner))
			polys.append(_p([[0.42, 0], [0.22, -0.07], [-0.2, -0.08], [-0.28, 0], [-0.2, 0.08], [0.22, 0.07]]))
			dots.append([Vector2(0.05, 0), 0.04])
		"galor":
			# Spade hull with two forward prongs.
			polys.append(_p([[0.3, 0], [0.12, -0.12], [-0.05, -0.3], [-0.4, -0.3], [-0.46, -0.18], [-0.46, 0.18], [-0.4, 0.3], [-0.05, 0.3], [0.12, 0.12]]))
			var prong := _p([[0.5, -0.09], [0.22, -0.1], [0.22, -0.05], [0.5, -0.06]])
			polys.append(prong)
			polys.append(_flip(prong))
			lines.append(_p([[0.28, 0], [-0.4, 0]]))
			lines.append(_p([[-0.05, -0.28], [-0.05, 0.28]]))
		"marauder":
			polys.append(_p([[0.5, 0], [0.34, -0.08], [0.12, -0.2], [-0.2, -0.22], [-0.46, -0.12], [-0.46, 0.12], [-0.2, 0.22], [0.12, 0.2], [0.34, 0.08]]))
			lines.append(PackedVector2Array(_arc(Vector2(0.02, 0), 0.12, 0, TAU, 18)))
			lines.append(_p([[0.34, 0], [0.5, 0]]))
			dots.append([Vector2(-0.3, 0), 0.05])
		"jemhadar":
			polys.append(_p([[0.2, 0], [0.04, -0.2], [-0.36, -0.28], [-0.46, -0.12], [-0.46, 0.12], [-0.36, 0.28], [0.04, 0.2]]))
			var claw := _p([[0.5, -0.17], [0.2, -0.16], [0.1, -0.1], [0.3, -0.1]])
			polys.append(claw)
			polys.append(_flip(claw))
			lines.append(_p([[0.15, 0], [-0.4, 0]]))
		"borg_cube":
			polys.append(_p([[-0.4, -0.4], [0.4, -0.4], [0.4, 0.4], [-0.4, 0.4]]))
			lines.append(_p([[-0.4, 0], [0.4, 0]]))
			lines.append(_p([[0, -0.4], [0, 0.4]]))
			lines.append(_p([[-0.2, -0.4], [-0.2, 0.4]]))
			lines.append(_p([[0.2, -0.4], [0.2, 0.4]]))
		"bioship":
			polys.append(_p([[0.5, 0], [0.2, -0.1], [0.0, -0.4], [-0.1, -0.1], [-0.45, -0.18], [-0.3, 0], [-0.45, 0.18], [-0.1, 0.1], [0.0, 0.4], [0.2, 0.1]]))
		_:
			# raider: a dart with swept rear fins
			polys.append(_p([[0.5, 0], [0.1, -0.12], [-0.3, -0.34], [-0.18, -0.08], [-0.4, 0], [-0.18, 0.08], [-0.3, 0.34], [0.1, 0.12]]))
	var d := {"polys": polys, "lines": lines, "dots": dots}
	_cache[family] = d
	return d

## Draws a hull silhouette. `length_px` is the bow-to-stern size; `angle` rotates the bow (0 = right, radians).
static func draw_hull(ci: CanvasItem, family: String, c: Vector2, length_px: float, angle: float, colour: Color,
		detail: bool = true, outline: bool = true, fill_alpha: float = 0.4) -> void:
	var sh := shape(family)
	var xf := Transform2D(angle, Vector2(length_px, length_px), 0.0, c)
	var lw := clampf(length_px / 60.0, 1.0, 2.4)
	for poly in sh["polys"]:
		var pts: PackedVector2Array = xf * poly
		if not Geometry2D.triangulate_polygon(pts).is_empty():
			ci.draw_colored_polygon(pts, Color(colour, fill_alpha))
		if outline:
			var loop := pts.duplicate()
			loop.append(pts[0])
			ci.draw_polyline(loop, colour, lw, true)
	if detail:
		for ln in sh["lines"]:
			ci.draw_polyline(xf * (ln as PackedVector2Array), Color(colour, 0.7), maxf(1.0, lw * 0.7), true)
		for dt in sh["dots"]:
			ci.draw_circle(xf * (dt[0] as Vector2), maxf(1.2, float(dt[1]) * length_px), colour)

## A starbase (ring, spokes, hub, docking arms) or an outpost (small hex with an antenna).
static func draw_station(ci: CanvasItem, c: Vector2, r: float, colour: Color, starbase: bool = true) -> void:
	var lw := clampf(r / 6.0, 1.2, 2.4)
	if starbase:
		var ring := PackedVector2Array()
		for i in range(8):
			ring.append(c + Vector2(cos(TAU * i / 8.0 + PI / 8.0), sin(TAU * i / 8.0 + PI / 8.0)) * r)
		ci.draw_colored_polygon(ring, Color(colour, 0.22))
		var loop := ring.duplicate()
		loop.append(ring[0])
		ci.draw_polyline(loop, colour, lw, true)
		for k in range(4):
			var a := TAU * k / 4.0 + PI / 4.0
			var d := Vector2(cos(a), sin(a))
			ci.draw_line(c + d * r * 0.3, c + d * r * 1.1, Color(colour, 0.85), lw, true)
			ci.draw_circle(c + d * r * 1.22, maxf(1.6, r * 0.2), colour)
		ci.draw_arc(c, r * 0.55, 0, TAU, 18, Color(colour, 0.8), lw * 0.8, true)
		ci.draw_circle(c, r * 0.3, colour)
	else:
		var hx := PackedVector2Array()
		for i in range(6):
			hx.append(c + Vector2(cos(TAU * i / 6.0), sin(TAU * i / 6.0)) * r * 0.78)
		ci.draw_colored_polygon(hx, Color(colour, 0.25))
		var l2 := hx.duplicate()
		l2.append(hx[0])
		ci.draw_polyline(l2, colour, lw, true)
		ci.draw_line(c, c + Vector2(0, -r * 1.25), colour, lw, true)
		ci.draw_circle(c + Vector2(0, -r * 1.25), maxf(1.4, r * 0.14), colour)
		ci.draw_circle(c, r * 0.22, colour)

## A foreign contact: targeting brackets round the lead hull's silhouette; "?" when nothing is known of it.
static func draw_contact(ci: CanvasItem, c: Vector2, r: float, colour: Color, owner: int = 0, klass: String = "") -> void:
	var lw := clampf(r / 6.0, 1.2, 2.2)
	var b := r * 1.15
	var t := r * 0.55
	for sx in [-1.0, 1.0]:
		for sy in [-1.0, 1.0]:
			var corner := c + Vector2(sx * b, sy * b)
			ci.draw_polyline(PackedVector2Array([corner + Vector2(-sx * t, 0), corner, corner + Vector2(0, -sy * t)]), Color(colour, 0.95), lw, true)
	if owner > 0:
		draw_hull(ci, family_for(owner, klass), c, r * 1.35, -PI / 2.0, colour, false, true, 0.55)
	else:
		var f := ThemeDB.fallback_font
		ci.draw_string(f, c + Vector2(-r * 0.3, r * 0.42), "?", HORIZONTAL_ALIGNMENT_LEFT, -1, int(r * 1.5), colour)
	ci.draw_circle(c + Vector2(b + 3.0, -b - 3.0), maxf(1.4, r * 0.14), Color(colour, 0.9))
