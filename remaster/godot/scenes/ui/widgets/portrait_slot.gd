class_name PortraitSlot
extends Control
## A race card or hail portrait (ART-BIBLE §7, docs/UI-DESIGN-SYSTEM.md §5, §7). Shows lane L7's portrait
## (res://art/portrait/<key>.*) when present; otherwise the empire emblem, large, on a gradient of the empire's
## colours with scan lines and a viewscreen frame, so the slot never looks empty.

var empire := 0
var key := ""
var caption := ""
var live := true      ## a slow scan line sweep (hail scenes); off under reduced motion
var _tex: Texture2D
var _t := 0.0

const MAJOR_KEYS := {1: "portrait.fed", 2: "portrait.kli", 3: "portrait.rom", 4: "portrait.car", 5: "portrait.fer"}

static func make(empire_: int, px: Vector2, key_: String = "", caption_: String = "") -> PortraitSlot:
	var p := PortraitSlot.new()
	p.empire = empire_
	p.key = key_ if key_ != "" else MAJOR_KEYS.get(empire_, "")
	p.caption = caption_
	p.custom_minimum_size = px
	p.mouse_filter = Control.MOUSE_FILTER_IGNORE
	return p

## Minor race key from its name ("Bajorans" -> portrait.minor.bajorans).
static func minor_key(name_: String) -> String:
	return "portrait.minor." + name_.to_lower().replace(" ", "").replace("'", "").replace("-", "")

func has_art() -> bool:
	return _tex != null

func _ready() -> void:
	_tex = AppLoader.art(key, "portrait")
	set_process(live and not UiTokens.reduced_motion)

func _process(delta: float) -> void:
	_t = fmod(_t + delta * 0.25, 1.0)
	queue_redraw()

func _draw() -> void:
	var r := Rect2(Vector2.ZERO, size)
	var col := Lcars.owner_color(empire) if empire > 0 else Lcars.accent()
	var rad := float(UiTokens.radius())
	var bg := StyleBoxFlat.new()
	bg.bg_color = Color(col.darkened(0.8), 1.0)
	bg.set_corner_radius_all(int(rad))
	bg.anti_aliasing = true
	draw_style_box(bg, r)
	if _tex:
		draw_texture_rect(_tex, r.grow(-2), false)
	else:
		# radial glow behind the emblem
		var c := size / 2.0
		for k in 8:
			var rr: float = min(size.x, size.y) * (0.62 - k * 0.06)
			draw_circle(c, rr, Color(col, 0.05 + k * 0.012))
		Lcars.draw_emblem(self, empire, c - Vector2(0, size.y * 0.04), min(size.x, size.y) * 0.56, Color(col.lightened(0.15), 0.92))
		for y in range(0, int(size.y), 4):
			draw_line(Vector2(4, y), Vector2(size.x - 4, y), Color(0, 0, 0, 0.18), 1.0)
	if live and not UiTokens.reduced_motion:
		var sy := size.y * _t
		draw_rect(Rect2(2, sy, size.x - 4, 3), Color(col.lightened(0.4), 0.10))
	var fr := StyleBoxFlat.new()
	fr.draw_center = false
	fr.border_color = Color(col, 0.75)
	fr.set_border_width_all(2)
	fr.set_corner_radius_all(int(rad))
	fr.anti_aliasing = true
	draw_style_box(fr, r)
	if caption != "":
		var f := Lcars.font("display", 600)
		var fs := 15
		draw_rect(Rect2(0, size.y - 26, size.x, 26), Color(0, 0, 0, 0.55))
		draw_string(f, Vector2(10, size.y - 8), caption.to_upper(), HORIZONTAL_ALIGNMENT_LEFT, size.x - 20, fs, col.lightened(0.3))
