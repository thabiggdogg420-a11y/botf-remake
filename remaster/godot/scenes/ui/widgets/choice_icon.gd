class_name ChoiceIcon
extends Control
## Construction choice glyph: the ship's own picture for starships (baked icon, else its hull family silhouette), a block for structures.

var product := ""
var ship := false
var owner_id := 1
var klass := ""
var asset_key := ""

func _draw() -> void:
	var c := size / 2.0
	var a := Lcars.accent()
	draw_circle(c, 16, Color(a, 0.15))
	if ship:
		var tex := ShipIcons.texture(owner_id, klass, "top", 64, asset_key)
		if tex != null:
			draw_texture_rect(tex, Rect2(c - Vector2(17, 17), Vector2(34, 34)), false)
		else:
			ShipGlyph.draw_hull(self, ShipGlyph.family_for(owner_id, klass), c, 28.0, 0.0, a, false, true, 0.35)
	else:
		draw_rect(Rect2(c - Vector2(8, 6), Vector2(16, 12)), a, false, 2.0)
		draw_rect(Rect2(c - Vector2(3, 11), Vector2(6, 5)), a)
