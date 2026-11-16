class_name ChoiceIcon
extends Control
## Construction choice glyph: the ship's own picture for starships (baked icon, else its hull family silhouette), and for a
## structure the Starfleet glyph of what it does (farm, replicator, laboratory, power, shield, sensor ...) on an LCARS tab plate.

var product := ""
var ship := false
var owner_id := 1
var klass := ""
var asset_key := ""
var label_name := ""   ## the structure's name, which picks its glyph

const KEYWORDS := [
	[["farm", "hydro", "aqua", "agri", "food", "forced-labor", "terraform"], "food"],
	[["factory", "foundry", "industr", "matter", "replicat", "manufact", "assembly"], "industry"],
	[["laborator", "research", "institute", "science", "theoretical", "tricorder", "analys"], "research"],
	[["power", "fusion", "reactor", "dilithium", "charge", "energy", "phoenix", "generator"], "energy"],
	[["shipyard", "dock", "yard", "drydock"], "ship"],
	[["defen", "battery", "bunker", "shield", "fortress", "orbital"], "shield"],
	[["communic", "listening", "scanner", "sensor", "subspace", "isolinear", "telescope"], "sensor"],
	[["simulator", "theatre", "theater", "entertain", "morale", "holo", "monument", "habitat", "medical", "hospital", "private"], "morale"],
	[["trade", "market", "commerce", "bank", "bazaar", "latinum", "exchange", "port"], "trade"],
	[["intel", "security", "spy", "obsidian", "tal shiar", "listening"], "intel"],
	[["academy", "barracks", "troop", "training", "garrison", "ground"], "population"],
]

static var _art := {}

## The structure's own illustration (res://assets/structures/<slug>.png, AI-generated concept art, see docs/ASSET-SOURCES.md), or
## null when there is none yet: the tile then falls back to the family glyph on a plate.
static func art(structure_name: String) -> Texture2D:
	var key := ShipIcons.slug(structure_name)
	if not _art.has(key):
		var path := "res://assets/structures/%s.png" % key
		_art[key] = load(path) as Texture2D if ResourceLoader.exists(path) else null
	return _art[key]

## Draws `tex` filling `r`, cropped to the box's aspect from the centre (never stretched).
static func draw_cover(ci: CanvasItem, tex: Texture2D, r: Rect2) -> void:
	var ts := tex.get_size()
	var k := maxf(r.size.x / ts.x, r.size.y / ts.y)
	var src_size := r.size / k
	ci.draw_texture_rect_region(tex, r, Rect2((ts - src_size) / 2.0, src_size))

static func glyph_for(structure_name: String) -> String:
	var n := structure_name.to_lower()
	for row in KEYWORDS:
		for kw in row[0]:
			if n.contains(kw):
				return row[1]
	return "build"

func _draw() -> void:
	if not ship:
		var tex := art(label_name)
		if tex != null:
			draw_cover(self, tex, Rect2(Vector2.ZERO, size))
			draw_rect(Rect2(Vector2.ZERO, size), Color(Pic.tint(glyph_for(label_name)), 0.5), false, 1.5)
			return
	var c := size / 2.0
	var a := Lcars.accent() if ship else Pic.tint(glyph_for(label_name))
	# the plate: an LCARS tab, round on the left and square on the right
	var h: float = min(size.x, size.y)
	var plate := Rect2(c - Vector2(h, h) / 2.0, Vector2(h, h))
	draw_circle(plate.position + Vector2(h * 0.5, h * 0.5), h * 0.5, Color(a, 0.16))
	draw_rect(Rect2(plate.position + Vector2(h * 0.5, 0), Vector2(h * 0.5, h)), Color(a, 0.16))
	if ship:
		var k := h / 34.0 # the 34 px plate is the design size; bigger plates scale the picture with it
		var tex := ShipIcons.texture(owner_id, klass, "top", 64 if h < 72.0 else 256, asset_key)
		if tex != null:
			draw_texture_rect(tex, Rect2(c - Vector2(17, 17) * k, Vector2(34, 34) * k), false)
		else:
			ShipGlyph.draw_hull(self, ShipGlyph.family_for(owner_id, klass), c, 28.0 * k, 0.0, a, false, true, 0.35)
	else:
		var g := h * 0.64
		UiIcon.draw_icon(self, glyph_for(label_name), Rect2(c - Vector2(g, g) / 2.0, Vector2(g, g)), a)
