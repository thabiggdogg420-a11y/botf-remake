class_name RingGauge
extends Control
## Donut gauge (docs/UI-DESIGN-SYSTEM.md §5). Single value: `value / max_value` in `color`. Segments: `segments` is
## [[value, Color]] and the ring shows their shares. The centre shows `center_text` over `sub_text`.

var value := 0.0
var max_value := 1.0
var color := Color.WHITE
var segments: Array = []
var center_text := ""
var sub_text := ""
var thickness := 10.0
var ex := {}

static func make(px: float, v: float, mx: float, c: Color, center: String = "", sub: String = "", explain: Variant = "") -> RingGauge:
	var g := RingGauge.new()
	g.custom_minimum_size = Vector2(px, px)
	g.value = v
	g.max_value = max(0.0001, mx)
	g.color = c
	g.center_text = center
	g.sub_text = sub
	g.thickness = clamp(px * 0.1, 5.0, 14.0)
	g.ex = RichTip.from(explain)
	g.tooltip_text = RichTip.text(g.ex) if str(explain) != "" else ""
	g.mouse_filter = Control.MOUSE_FILTER_PASS
	g.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	g.size_flags_horizontal = Control.SIZE_SHRINK_CENTER
	return g

func _make_custom_tooltip(_for_text: String) -> Object:
	return RichTip.build(ex)

func _draw() -> void:
	var c := size / 2.0
	var r: float = min(size.x, size.y) / 2.0 - thickness / 2.0 - 1.0
	draw_arc(c, r, 0, TAU, 64, Lcars.LINE2, thickness, true)
	var start := -PI / 2.0
	if segments.size() > 0:
		var tot := 0.0
		for s in segments:
			tot += max(0.0, float(s[0]))
		if tot > 0:
			for s in segments:
				var span: float = TAU * max(0.0, float(s[0])) / tot
				if span > 0.01:
					draw_arc(c, r, start + 0.02, start + span - 0.02, max(4, int(64 * span / TAU)), s[1], thickness, true)
				start += span
	else:
		var f: float = clamp(value / max_value, 0.0, 1.0)
		if f > 0.0:
			draw_arc(c, r, start, start + TAU * f, max(4, int(64 * f)), color, thickness, true)
			draw_circle(c + Vector2(cos(start + TAU * f), sin(start + TAU * f)) * r, thickness * 0.55, color.lightened(0.25))
	var fnt := Lcars.font("display", 600)
	var fs := int(clamp(r * 0.62, 12, 40))
	if center_text != "":
		var w := fnt.get_string_size(center_text, HORIZONTAL_ALIGNMENT_LEFT, -1, fs).x
		var y := c.y + fs * 0.32 - (fs * 0.28 if sub_text != "" and min(size.x, size.y) >= 80 else 0.0)
		draw_string(fnt, Vector2(c.x - w / 2.0, y), center_text, HORIZONTAL_ALIGNMENT_LEFT, -1, fs, Lcars.TEXT)
	if sub_text != "" and min(size.x, size.y) >= 80:
		var ss := int(clamp(r * 0.26, 9, 14))
		var w2 := fnt.get_string_size(sub_text, HORIZONTAL_ALIGNMENT_LEFT, -1, ss).x
		draw_string(fnt, Vector2(c.x - w2 / 2.0, c.y + fs * 0.55), sub_text, HORIZONTAL_ALIGNMENT_LEFT, -1, ss, Lcars.TEXT3)
