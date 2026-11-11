class_name GalaxyOverlay
extends Control
## Screen-space layer of the 3D galaxy map: lines in space (sector grid, drop lines, trade routes, wormhole links,
## fleet paths), icons (ownership rings, minor races, stations, contacts, own fleets) and labels, all sized in pixels
## so they read at every zoom. Two instances: the static layer (anim = false) redraws only when the camera or the
## data changes and records what can be picked; the animated layer (anim = true) redraws every frame and holds only
## what moves (selection pulses, flowing route dashes, the neutron star beam, planet orbits and their labels).
## Text is shaped once and cached (TextLine): shaping 80 labels every frame cost about 10 ms.

## Threat actors (docs/design/THREAT-ACTORS.md): marker colours (Borg green, Species 8472 violet, monsters alert red, peaceful ones
## green-white) and the existing insignia each actor wears. Husnock, Chodak, the Crystalline Entity, the Edo Guardian and the rest
## borrow another faction's mark where the roster has none of their own (stand-ins listed in the design doc).
const THREAT_BORG := Color("#5cff7a")
const THREAT_8472 := Color("#c58bff")
const THREAT_INSIGNIA := {"borg_cube": "borg", "borg_sphere": "borg", "species_8472": "species8472", "husnock": "krenim", "calamarain": "neutral",
	"chodak": "kazon", "crystalline": "xindi", "echo_papa": "borg", "edo_guardian": "krenim", "gomtuu": "neutral", "tarellian": "minor",
	"deep_space_9": "federation"}

var map: GalaxyMap
var anim := false
## Static layer: what the last draw put on screen that can be picked: {"pos", "r", "cell", "kind", "id"}.
var pickables: Array = []
## Static layer: rings of the player's systems, for the animated arc: [pos, radius, colour].
var player_rings: Array = []
var _xf_inv := Transform3D.IDENTITY
var _near := 0.05
var _text_cache := {}

## A shaped line of text, cached.
func _line(font: Font, text: String, fs: int) -> TextLine:
	var key := "%d|%d|%s" % [font.get_instance_id(), fs, text]
	var tl: TextLine = _text_cache.get(key)
	if tl == null:
		if _text_cache.size() > 3000:
			_text_cache.clear()
		tl = TextLine.new()
		tl.add_string(text, font, fs)
		_text_cache[key] = tl
	return tl

func _tw(font: Font, text: String, fs: int) -> float:
	return _line(font, text, fs).get_size().x

## Draws text with its baseline at `pos` (like draw_string); outline > 0 draws only the outline.
func _txt(font: Font, pos: Vector2, text: String, fs: int, col: Color, outline: int = 0) -> void:
	var tl := _line(font, text, fs)
	var top := pos - Vector2(0, tl.get_line_ascent())
	if outline > 0:
		tl.draw_outline(get_canvas_item(), top, outline, col)
	else:
		tl.draw(get_canvas_item(), top, col)

func _cam_z(w: Vector3) -> float:
	return (_xf_inv * w).z

func front(w: Vector3) -> bool:
	return _cam_z(w) < -_near * 1.5

## A world segment clipped to the front of the camera, as two screen points ([] when fully behind).
func seg(a: Vector3, b: Vector3) -> PackedVector2Array:
	var za := _cam_z(a)
	var zb := _cam_z(b)
	var lim := -_near * 1.5
	if za > lim and zb > lim:
		return PackedVector2Array()
	if za > lim:
		a = a.lerp(b, (za - lim) / (za - zb))
	elif zb > lim:
		b = b.lerp(a, (zb - lim) / (zb - za))
	return PackedVector2Array([map.world_to_screen(a), map.world_to_screen(b)])

func on_screen(p: Vector2, margin: float = 60.0) -> bool:
	return p.x > -margin and p.y > -margin and p.x < size.x + margin and p.y < size.y + margin

func _draw() -> void:
	var v := map.view()
	if v.is_empty() or map._camera == null:
		if not anim:
			pickables.clear()
		return
	_xf_inv = map._camera.global_transform.affine_inverse()
	_near = map._camera.near
	if anim:
		_draw_anim(v)
	else:
		_draw_static(v)

# ---------------------------------------------------------------------------------------------
# Static layer
# ---------------------------------------------------------------------------------------------
## A size per zoom band: [whole galaxy, sector, system].
static func _band(level: String, galaxy: float, sector: float, system: float) -> float:
	return galaxy if level == "galaxy" else (sector if level == "sector" else system)

func _draw_static(v: Dictionary) -> void:
	pickables.clear()
	player_rings.clear()
	var app := map.app
	var g := map.grid_size()
	var accent := Lcars.accent()
	var lvl := map.zoom_level()
	var cp := map.camera_position()
	var mono := Lcars.font("mono")
	# ---- Sector grid on the galactic plane, and the charted edge
	if map.layers.get("grid", true):
		var ga := clampf(0.06 - 0.01 * maxf(map.zoom - 2.0, 0.0), 0.015, 0.06)
		var gc := Color(0.55, 0.67, 0.9, ga)
		for x in range(g.x + 1):
			var s := seg(Vector3(x, 0, 0), Vector3(x, 0, g.y))
			if s.size() == 2:
				draw_line(s[0], s[1], gc, 1.0)
		for y in range(g.y + 1):
			var s2 := seg(Vector3(0, 0, y), Vector3(g.x, 0, y))
			if s2.size() == 2:
				draw_line(s2[0], s2[1], gc, 1.0)
	var corners := [Vector3(0, 0, 0), Vector3(g.x, 0, 0), Vector3(g.x, 0, g.y), Vector3(0, 0, g.y)]
	for k in 4:
		var e := seg(corners[k], corners[(k + 1) % 4])
		if e.size() == 2:
			draw_line(e[0], e[1], Color(accent, 0.28), 1.5, true)
	# ---- Drop lines: every star tied to its footprint on the plane, so height reads
	if lvl != "system":
		var up_c := Color(0.6, 0.75, 1.0, 0.22)
		var down_c := Color(0.6, 0.75, 1.0, 0.12)
		var foot_c := Color(0.6, 0.75, 1.0, 0.3)
		for sid in map.systems3d:
			var pos: Vector3 = map.systems3d[sid]["pos"]
			var s3 := seg(pos, Vector3(pos.x, 0.0, pos.z))
			if s3.size() == 2 and on_screen(s3[0]):
				draw_line(s3[0], s3[1], up_c if pos.y >= 0.0 else down_c, 1.0)
				draw_circle(s3[1], 1.6, foot_c)
	# ---- Systems: ownership ring, minor-race diamond, name (labels that would overlap a higher-ranked one are skipped)
	var f_disp := Lcars.font("display", 560)
	var fs := int(clampf(12.0 + 3.0 * log(maxf(map.zoom, 0.2) / 0.5) / log(2.0), 12.0, 20.0))
	var taken: Array = []
	var ranked: Array = []
	for sid in map.systems3d:
		ranked.append([_label_rank(sid), sid])
	ranked.sort_custom(func(a, b): return a[0] > b[0])
	var owner_cols := {}
	var labels_on: bool = map.layers.get("labels", true)
	# Every on-screen star's ring is an obstacle for labels, so a label never lands on another star or on another label.
	var discs: Array = []
	for r0 in ranked:
		var e0: Dictionary = map.systems3d[r0[1]]
		if front(e0["pos"]):
			var p0 := map.world_to_screen(e0["pos"])
			if on_screen(p0, 120.0):
				var st0: Dictionary = GalaxyLayout.STAR_TYPES[int(e0["type"])]
				discs.append([r0[1], p0, maxf(13.0, float(st0["radius"]) * map.pixels_per_unit(cp.distance_to(e0["pos"])) * 1.6) + 2.0])
	for r in ranked:
		var sid: int = r[1]
		var e2: Dictionary = map.systems3d[sid]
		var s: Dictionary = e2["system"]
		var pos2: Vector3 = e2["pos"]
		if not front(pos2):
			continue
		var p := map.world_to_screen(pos2)
		if not on_screen(p, 120.0):
			continue
		var st: Dictionary = GalaxyLayout.STAR_TYPES[int(e2["type"])]
		var sphere_px := float(st["radius"]) * map.pixels_per_unit(cp.distance_to(pos2))
		var ring_r := maxf(13.0, sphere_px * 1.6)
		pickables.append({"pos": p, "r": maxf(0.0, sphere_px - 6.0), "cell": int(e2["cell"]), "kind": "system", "id": sid})
		var charted := AppSession.is_charted(s)
		var owner := int(s.get("owner", 0)) if charted else 0
		if owner != 0 and not owner_cols.has(owner):
			owner_cols[owner] = Lcars.owner_color(owner)
		if owner != 0:
			var oc: Color = owner_cols[owner]
			draw_arc(p, ring_r, 0, TAU, 32, Color(oc, 0.85), 2.0, true)
			if owner == app.player():
				player_rings.append([p, ring_r + 4.0, oc])
			var omk := Vector2(ring_r * 0.72, -ring_r * 0.72) + Vector2(7, -7)
			Insignia.draw(self, owner, Rect2(p + omk - Vector2(8, 8), Vector2(16, 16)), Color(oc, 0.95), "small", Lcars.BG)
		elif not charted:
			# Uncharted: the star is on the chart, nothing else is known. A neutral dim ring marks it.
			draw_arc(p, ring_r, 0, TAU, 32, Color(0.62, 0.72, 0.9, 0.42), 1.5, true)
		var minor: Dictionary = s.get("minor", {}) if s.has("minor") else {}
		if not minor.is_empty():
			var mc := Lcars.owner_color(int(minor.get("memberOf", 0))) if int(minor.get("memberOf", 0)) != 0 else Color("#d8c3ff")
			var mk := Vector2(ring_r * 0.72, -ring_r * 0.72) + Vector2(7, -7)
			Insignia.draw(self, "minor", Rect2(p + mk - Vector2(8, 8), Vector2(16, 16)), Color(mc, 0.95), "small", Lcars.BG)
			if minor.has("guard"):
				draw_arc(p + mk, 11.0, 0, TAU, 24, Color(Lcars.ALERT, 0.9), 1.6, true) # a signature ship stands guard
		if labels_on:
			var col: Color = owner_cols[owner] if owner != 0 else (Color(Lcars.TEXT2, 0.9) if charted else Color(Lcars.TEXT3, 0.85))
			var name_ := str(s["name"]).to_upper()
			var w := _tw(f_disp, name_, fs)
			# Try below the ring, then above, then to the right and left; hide the label when none is free.
			var spots := [Vector2(-w / 2.0, ring_r + 6.0 + fs), Vector2(-w / 2.0, -ring_r - 6.0),
				Vector2(ring_r + 6.0, fs * 0.35), Vector2(-ring_r - 6.0 - w, fs * 0.35)]
			var lp := Vector2.ZERO
			var rect := Rect2()
			var placed := false
			for off in spots:
				lp = p + off
				rect = Rect2(lp + Vector2(-2, -fs), Vector2(w + 4, fs + 4))
				var clash := false
				for t in taken:
					if rect.intersects(t):
						clash = true
						break
				if not clash:
					for d in discs:
						if int(d[0]) != sid and rect.intersects(Rect2(d[1] - Vector2(d[2], d[2]), Vector2(d[2], d[2]) * 2.0)):
							clash = true
							break
				if not clash:
					placed = true
					break
			if not placed:
				continue
			taken.append(rect)
			_txt(f_disp, lp, name_, fs, Color(0, 0, 0, 0.8), 4)
			_txt(f_disp, lp, name_, fs, col)
			if lvl == "system" and sphere_px > 6.0:
				var sub := str(st["name"]).to_upper()
				var sw := _tw(mono, sub, 12)
				_txt(mono, lp + Vector2((w - sw) / 2.0, 16), sub, 12, Color(0, 0, 0, 0.8), 3)
				_txt(mono, lp + Vector2((w - sw) / 2.0, 16), sub, 12, Color(Lcars.TEXT3, 0.9))
	# ---- Unsurveyed anomaly marks
	for f in v.get("features", []):
		if f.get("surveyed", true):
			continue
		var fw := Vector3(int(f["x"]) + 0.5, 0.0, int(f["y"]) + 0.5)
		if front(fw):
			_txt(Lcars.font("display", 600), map.world_to_screen(fw) + Vector2(12, -12), "?", 16, Color(1, 1, 1, 0.75))
	# ---- Stations
	for st2 in v.get("stations", []):
		var cell := int(st2["y"]) * g.x + int(st2["x"])
		var aw := map.cell_anchor(cell)
		if not front(aw):
			continue
		var sp := map.world_to_screen(aw) + Vector2(-22, 18)
		var sc2 := Lcars.owner_color(int(st2["owner"]))
		var rad := (8.0 if st2["type"] == "Outpost" else 11.0) * _band(lvl, 1.35, 1.2, 1.0)
		ShipGlyph.draw_station(self, sp, rad, sc2, st2["type"] != "Outpost")
		if st2.has("upgrade"):
			draw_arc(sp, rad + 5, -PI / 2, -PI / 2 + TAU * float(st2["upgrade"]["work"]) / maxf(1.0, float(st2["upgrade"]["cost"])), 24, Lcars.GOOD, 2.0, true)
		pickables.append({"pos": sp, "r": rad - 4.0, "cell": cell, "kind": "station", "id": int(st2.get("id", 0))})
	# ---- Fleet path ends with ETAs (the dashes flow in the animated layer)
	if map.layers.get("routes", true):
		for fl in v.get("fleets", []):
			var route: Array = fl.get("route", [])
			if route.size() < 2:
				continue
			var rc := _route_color(fl)
			var endw := map.cell_anchor(int(route[route.size() - 1]))
			if not front(endw):
				continue
			var end := map.world_to_screen(endw)
			draw_arc(end, 10, 0, TAU, 24, rc, 2.0, true)
			draw_circle(end, 3, rc)
			if int(fl.get("eta", 0)) > 0:
				var txt := "%dT" % int(fl["eta"])
				var tw := _tw(mono, txt, 13)
				var p2 := end + Vector2(14, -14)
				draw_style_box(Lcars.box(Color(Lcars.BG, 0.85), 6, Lcars.accent2(), 1, Vector4.ZERO), Rect2(p2 + Vector2(-4, -13), Vector2(tw + 8, 17)))
				_txt(mono, p2, txt, 13, Lcars.accent2())
	# ---- Contacts
	var by_cell := {}
	for c in v.get("contacts", []):
		var key := int(c["y"]) * g.x + int(c["x"])
		var aw2 := map.cell_anchor(key)
		if not front(aw2):
			continue
		by_cell[key] = by_cell.get(key, 0) + 1
		var cp2 := map.world_to_screen(aw2) + Vector2(22, 16) + Vector2(-(by_cell[key] - 1) * 9, 0)
		ShipGlyph.draw_contact(self, cp2, 8.0 * _band(lvl, 1.4, 1.25, 1.0), Lcars.owner_color(int(c["owner"])), int(c["owner"]), ShipGlyph.class_of(c))
		pickables.append({"pos": cp2, "r": 0.0, "cell": key, "kind": "contact", "id": int(c["id"]), "war": c.get("atWar", false)})
	# ---- Systems the Borg hold (assimilated), then threat actors (docs/design/THREAT-ACTORS.md)
	for m in v.get("assimilated", []):
		var mkey := int(m["y"]) * g.x + int(m["x"])
		var maw := map.cell_anchor(mkey)
		if not front(maw):
			continue
		var mp := map.world_to_screen(maw)
		draw_arc(mp, 20.0, 0, TAU, 28, Color(THREAT_BORG, 0.8), 2.0, true)
		draw_arc(mp, 24.0, 0, TAU, 28, Color(THREAT_BORG, 0.35), 1.0, true)
		Insignia.draw(self, "borg", Rect2(mp + Vector2(-8, 18), Vector2(16, 16)), Color(THREAT_BORG, 0.95), "small", Lcars.BG)
	var threat_by_cell := {}
	for a in v.get("threats", []):
		var tkey := int(a["y"]) * g.x + int(a["x"])
		var taw := map.cell_anchor(tkey)
		if not front(taw):
			continue
		threat_by_cell[tkey] = threat_by_cell.get(tkey, 0) + 1
		var tp := map.world_to_screen(taw) + Vector2(-24 - (threat_by_cell[tkey] - 1) * 26, -22)
		var tc := THREAT_BORG if str(a.get("group", "")) == "borg" else (THREAT_8472 if str(a.get("group", "")) == "8472" else Lcars.ALERT)
		if not bool(a.get("hostile", true)):
			tc = Lcars.GOOD
		var tr := _band(lvl, 15.0, 16.0, 18.0)
		draw_circle(tp, tr + 3.0, Color(Lcars.BG, 0.9))
		draw_arc(tp, tr + 3.0, 0, TAU, 32, tc, 2.4, true)
		draw_arc(tp, tr + 7.0, -PI / 2.0, -PI / 2.0 + TAU * clampf(float(a["hull"]) / maxf(1.0, float(a["maxHull"])), 0.0, 1.0), 32, Color(tc, 0.55), 2.0, true)
		Insignia.draw(self, THREAT_INSIGNIA.get(str(a["key"]), "neutral"), Rect2(tp - Vector2(tr, tr) * 0.8, Vector2(tr, tr) * 1.6), tc, "small", Lcars.BG)
		if lvl != "galaxy":
			var tn := str(a["name"]).to_upper()
			_txt(mono, tp + Vector2(-_tw(mono, tn, 11) / 2.0, tr + 20.0), tn, 11, Color(0, 0, 0, 0.8), 3)
			_txt(mono, tp + Vector2(-_tw(mono, tn, 11) / 2.0, tr + 20.0), tn, 11, tc)
		pickables.append({"pos": tp, "r": tr, "cell": tkey, "kind": "space", "id": 0})
	# ---- Own fleets
	var own := {}
	for f in v.get("fleets", []):
		var key2 := int(f["y"]) * g.x + int(f["x"])
		if not own.has(key2):
			own[key2] = []
		own[key2].append(f)
	var oc2 := Lcars.owner_color(app.player())
	for key3 in own:
		var list: Array = own[key3]
		var f0: Dictionary = list[0]
		var aw3 := map.cell_anchor(key3)
		if not front(aw3):
			continue
		var at := map.world_to_screen(aw3)
		var base := at + Vector2(22, -20)
		var heading := Vector2.RIGHT
		var route2: Array = f0.get("route", [])
		if route2.size() > 1:
			var nx := map.world_to_screen(map.cell_anchor(int(route2[1])))
			if (nx - at).length() > 1.0:
				heading = (nx - at).normalized()
		var tf := false
		var ids: Array = []
		for f in list:
			ids.append(int(f["id"]))
			if int(f.get("taskForceId", 0)) != 0:
				tf = true
		# A hull that reads as a family at every zoom: never under ~54 px long, larger on the whole-galaxy view where nothing
		# else is big, with the owner's insignia chip beside the stack count.
		var hl: float = _band(lvl, 58.0, 62.0, 74.0)
		ShipGlyph.draw_hull(self, ShipGlyph.family_for(app.player(), ShipGlyph.class_of(f0)), base, hl, heading.angle(), oc2, true, true, 0.5)
		if tf:
			draw_arc(base, hl * 0.56, 0, TAU, 24, Color(oc2, 0.8), 1.5, true)
		var chip := base + Vector2(hl * 0.40, hl * 0.34)
		draw_circle(chip, 11.0, Color(Lcars.BG, 0.88))
		draw_arc(chip, 11.0, 0, TAU, 24, Color(oc2, 0.9), 1.2, true)
		Insignia.draw(self, app.player(), Rect2(chip - Vector2(8, 8), Vector2(16, 16)), oc2, "small", Lcars.BG)
		if list.size() > 1:
			var ct := "x%d" % list.size()
			var cw := _tw(mono, ct, 13)
			draw_style_box(Lcars.box(Color(Lcars.BG, 0.88), 6, Color(oc2, 0.9), 1, Vector4.ZERO), Rect2(chip + Vector2(12, -8), Vector2(cw + 8, 17)))
			_txt(mono, chip + Vector2(16, 5), ct, 13, Color.WHITE)
		if f0.get("routeBlocked", false):
			draw_arc(base, 14, 0, TAU, 20, Lcars.WARN, 1.5, true)
		pickables.append({"pos": base, "r": 0.0, "cell": key3, "kind": "fleet", "id": int(f0["id"]), "ids": ids})
	# ---- Sector coordinates along the charted edge
	if map.layers.get("grid", true) and map.zoom > 0.8 and lvl != "system":
		var cf := Color(Lcars.TEXT3, 0.4)
		for x in range(g.x):
			var w0 := Vector3(x + 0.5, 0, -0.25)
			if front(w0):
				var p3 := map.world_to_screen(w0)
				if on_screen(p3, 0.0):
					_txt(mono, p3 + Vector2(-6, 4), str(x), 11, cf)
		for y in range(g.y):
			var w1 := Vector3(-0.3, 0, y + 0.5)
			if front(w1):
				var p4 := map.world_to_screen(w1)
				if on_screen(p4, 0.0):
					_txt(mono, p4 + Vector2(-10, 4), str(y), 11, cf)
	# ---- Zoom level readout (bottom left, above the layer bar)
	var lv := "GALAXY" if lvl == "galaxy" else ("SECTOR" if lvl == "sector" else "SYSTEM")
	_txt(mono, Vector2(18, size.y - 96), "%s  %.1fx" % [lv, snappedf(map.zoom, 0.1)], 12, Color(Lcars.TEXT3, 0.7))

## Label priority: selected sector, then the player's systems, other owners, minors, the rest.
func _label_rank(sid: int) -> int:
	var e: Dictionary = map.systems3d[sid]
	var s: Dictionary = e["system"]
	var rank := 0
	if int(e["cell"]) == int(map.app.selection.get("cell", -1)):
		rank += 100
	if not AppSession.is_charted(s):
		return rank
	var owner := int(s.get("owner", 0))
	if owner == map.app.player():
		rank += 50
	elif owner != 0:
		rank += 30
	elif s.has("minor"):
		rank += 20
	return rank

func _route_color(fl: Dictionary) -> Color:
	var app := map.app
	var selected := int(fl["id"]) == int(app.selection.get("fleetId", 0)) or (int(fl.get("taskForceId", 0)) != 0 and int(fl["taskForceId"]) == int(app.selection.get("taskForceId", 0)))
	return Lcars.WARN if fl.get("routeBlocked", false) else (Lcars.accent2() if selected else Color(Lcars.accent(), 0.8))

# ---------------------------------------------------------------------------------------------
# Animated layer
# ---------------------------------------------------------------------------------------------
func _draw_anim(v: Dictionary) -> void:
	var app := map.app
	var g := map.grid_size()
	var t := Time.get_ticks_msec() / 1000.0
	var accent := Lcars.accent()
	var cp := map.camera_position()
	var stat: GalaxyOverlay = map._overlay
	# ---- Hover and selection sectors (outlined at the sector's anchor height)
	var sel := int(app.selection.get("cell", -1))
	if map.hover_cell >= 0:
		_cell_quad(map.hover_cell, g, Color(1, 1, 1, 0.12), 1.0)
	if sel >= 0:
		_cell_quad(sel, g, Color(accent, 0.55 + 0.25 * sin(t * 3.0)), 2.0)
	# ---- Trade routes
	if map.layers.get("trade", true):
		for tr in v.get("tradeRoutes", []):
			var a := app.find_by_id("systems", int(tr["from"]))
			var b := app.find_by_id("systems", int(tr["to"]))
			if a.is_empty() or b.is_empty():
				continue
			var pa := map.cell_anchor(int(a["y"]) * g.x + int(a["x"]))
			var pb := map.cell_anchor(int(b["y"]) * g.x + int(b["x"]))
			_curve(pa, pb, 0.25, Color(Lcars.owner_color(int(tr["owner"])), 0.55), 2.0, 6.0, 8.0, t * 10.0)
	# ---- Wormhole links: an arc lifted out of the plane
	for f in v.get("features", []):
		if f["type"] == "Wormhole" and f.has("exit") and int(f["id"]) < int(f.get("pairId", 0)):
			var w0 := Vector3(int(f["x"]) + 0.5, 0.0, int(f["y"]) + 0.5)
			var w1 := Vector3(int(f["exit"]["x"]) + 0.5, 0.0, int(f["exit"]["y"]) + 0.5)
			_curve(w0, w1, 1.4, Color(0.45, 0.85, 1.0, 0.45), 2.0, 10.0, 6.0, t * 12.0)
	# ---- Neutron star beam
	for f in v.get("features", []):
		if str(f["type"]) != "NeutronStar":
			continue
		var fw := Vector3(int(f["x"]) + 0.5, 0.0, int(f["y"]) + 0.5)
		if front(fw):
			var fp := map.world_to_screen(fw)
			var pl := 0.5 + 0.5 * sin(t * 6.0)
			var beam := Vector2(cos(t * 1.5), sin(t * 1.5)) * (22.0 + 10.0 * pl)
			draw_line(fp - beam, fp + beam, Color(0.8, 0.95, 1.0, 0.75), 2.0, true)
	# ---- Fleet paths: flowing dashes between the anchors of each step
	if map.layers.get("routes", true):
		for fl in v.get("fleets", []):
			var route: Array = fl.get("route", [])
			if route.size() < 2:
				continue
			var selected := int(fl["id"]) == int(app.selection.get("fleetId", 0))
			var rc := stat._route_color(fl)
			for k in range(route.size() - 1):
				_curve(map.cell_anchor(int(route[k])), map.cell_anchor(int(route[k + 1])), 0.0, rc, 3.0 if selected else 2.0, 9.0, 7.0, -t * 26.0)
	# ---- Player system arcs, contact war pulse, selection rings
	for r in stat.player_rings:
		draw_arc(r[0], r[1], -t, -t + PI * 0.6, 16, Color(r[2], 0.7), 1.5, true)
	var fid := int(app.selection.get("fleetId", 0))
	for c in stat.pickables:
		if c["kind"] == "contact":
			if c.get("war", false):
				draw_arc(c["pos"], 12, 0, TAU, 20, Color(Lcars.ALERT, 0.6 + 0.3 * sin(t * 4.0)), 2.0, true)
			if int(c["id"]) == fid:
				draw_arc(c["pos"], 15, 0, TAU, 24, Color.WHITE, 2.0, true)
		elif c["kind"] == "fleet" and fid != 0 and (c["ids"] as Array).has(fid):
			draw_arc(c["pos"], 16.0 + 2.0 * sin(t * 5.0), 0, TAU, 28, Color.WHITE, 2.0, true)
	# ---- Orbits and planet labels at system zoom (planets move every frame)
	if map.zoom >= GalaxyMap.LEVEL_SYSTEM * 0.6:
		_draw_orbits(cp)

func _cell_quad(cell: int, g: Vector2i, col: Color, w: float) -> void:
	var x := cell % g.x
	var y := cell / g.x
	var h := map.cell_anchor(cell).y
	var pts := [Vector3(x + 0.04, h, y + 0.04), Vector3(x + 0.96, h, y + 0.04), Vector3(x + 0.96, h, y + 0.96), Vector3(x + 0.04, h, y + 0.96)]
	for k in 4:
		var s := seg(pts[k], pts[(k + 1) % 4])
		if s.size() == 2:
			draw_line(s[0], s[1], col, w, true)

func _draw_orbits(cp: Vector3) -> void:
	var mono := Lcars.font("mono")
	for sid in map.systems3d:
		var e: Dictionary = map.systems3d[sid]
		var pr: Node3D = e["planets"]
		if pr == null or not pr.visible:
			continue
		var c: Vector3 = e["pos"]
		for pivot in pr.get_children():
			var r := float(pivot.get_meta("radius"))
			var pts := PackedVector2Array()
			for k in range(65):
				var a := k * TAU / 64.0
				var w := c + Vector3(cos(a) * r, 0.0, sin(a) * r)
				if front(w):
					pts.append(map.world_to_screen(w))
			if pts.size() > 2:
				draw_polyline(pts, Color(0.6, 0.75, 1.0, 0.16), 1.0, true)
			var p: Dictionary = pivot.get_meta("planet")
			var body := pivot.get_child(0) as Node3D
			var bw := body.global_position if body.name == "Planet" else c + (pivot as Node3D).global_transform.basis * Vector3(r, 0, 0)
			if not front(bw):
				continue
			var bp := map.world_to_screen(bw)
			var rad := float(GalaxyMap.PLANET_RADIUS.get(str(p.get("class", "K")), 0.011)) * map.pixels_per_unit(cp.distance_to(bw))
			if map.zoom >= GalaxyMap.LEVEL_SYSTEM and on_screen(bp, 0.0):
				var cls := str(p.get("class", ""))
				var label := "%s  %s" % [str(p.get("name", "")), "GAS GIANT" if cls == "GasGiant" else ("BELT" if cls == "Asteroids" else "CLASS " + cls)]
				var lp := bp + Vector2(maxf(rad, 4.0) + 8.0, 4.0)
				_txt(mono, lp, label, 12, Color(0, 0, 0, 0.85), 3)
				_txt(mono, lp, label, 12, Color(0.78, 0.86, 1.0, 0.9))

## A dashed line between two points in space; `lift` raises the middle into an arc (sectors).
func _curve(a: Vector3, b: Vector3, lift: float, col: Color, w: float, dash: float, gap: float, phase: float) -> void:
	var n := 12 if lift > 0.0 else 1
	var mid := (a + b) * 0.5 + Vector3(0, lift, 0)
	var acc := phase
	for k in n:
		var u0 := float(k) / n
		var u1 := float(k + 1) / n
		var p0 := a.lerp(mid, u0).lerp(mid.lerp(b, u0), u0) if lift > 0.0 else a
		var p1 := a.lerp(mid, u1).lerp(mid.lerp(b, u1), u1) if lift > 0.0 else b
		var s := seg(p0, p1)
		if s.size() == 2:
			acc = _dashed(s[0], s[1], col, w, dash, gap, acc)

func _dashed(a: Vector2, b: Vector2, col: Color, w: float, dash: float, gap: float, phase: float) -> float:
	var len := a.distance_to(b)
	if len < 0.01:
		return phase
	if not (on_screen(a, 400.0) or on_screen(b, 400.0)):
		return phase - len
	var dir := (b - a) / len
	var period := dash + gap
	var s := fposmod(phase, period) - period
	while s < len:
		var s0: float = maxf(s, 0.0)
		var s1: float = minf(s + dash, len)
		if s1 > s0:
			draw_line(a + dir * s0, a + dir * s1, col, w, true)
		s += period
	return phase - len
