class_name ShipPicture
extends Control
## A picture of one ship class for menus: the baked 3D icon when LocalAssets has one, else the in-house vector hull of the
## class's family (ShipGlyph). Set owner_id and klass (or call ShipPicture.make).

var owner_id := 1
var klass := ""
var asset_key := ""
var view := "iso"
var color := Color.WHITE
var plate := false  ## a faint panel and grid behind it (the hero cards)

static func make(owner: int, klass_: String, px: float, view_: String = "iso", tint: Variant = null, asset_key_: String = "") -> ShipPicture:
	var p := ShipPicture.new()
	p.owner_id = owner
	p.klass = klass_
	p.view = view_
	p.asset_key = asset_key_
	p.color = Lcars.owner_color(owner) if tint == null else tint
	p.custom_minimum_size = Vector2(px, px)
	p.mouse_filter = Control.MOUSE_FILTER_IGNORE
	p.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	return p

func _draw() -> void:
	if plate:
		draw_rect(Rect2(Vector2.ZERO, size), Color(Lcars.BG2, 0.6))
		for k in range(0, int(size.x), 24):
			draw_line(Vector2(k, 0), Vector2(k, size.y), Color(1, 1, 1, 0.03))
	var s := minf(size.x, size.y)
	var tex := ShipIcons.texture(owner_id, klass, view, 256 if s > 72.0 else 64, asset_key)
	if tex != null:
		var side := s if not plate else s * 0.96
		draw_texture_rect(tex, Rect2((size - Vector2(side, side)) / 2.0, Vector2(side, side)), false)
		return
	var fam := ShipGlyph.family_for(owner_id, klass)
	var len_px := minf(size.x * 0.86, size.y * 1.5) if plate else s * 0.9
	ShipGlyph.draw_hull(self, fam, size / 2.0, len_px, 0.0, color, true, true, 0.3)
