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
	# An LCARS arc gauge: a ring of blocks with black gaps and quarter ticks inside, not a plain donut.
	var n: int = 24 if r < 24.0 else (36 if r < 44.0 else 48)
	var step := TAU / n
	var gap: float = clamp(2.2 / max(1.0, r), 0.02, 0.07)
	var start := -PI / 2.0
	var shares: Array = []   # per block: its colour, or a clear colour when the block is empty
	if segments.size() > 0:
		var tot := 0.0
		for sg in segments:
			tot += max(0.0, float(sg[0]))
		var acc := 0.0
		for i in n:
			var mid := (i + 0.5) / n
			var col := Color(0.55, 0.67, 0.9, 0.2)
			var lo := 0.0
			for sg in segments:
				var span: float = (max(0.0, float(sg[0])) / tot) if tot > 0.0 else 0.0
				if mid >= lo and mid < lo + span:
					col = sg[1]
					break
				lo += span
			shares.append(col)
	else:
		var f: float = clamp(value / max_value, 0.0, 1.0)
		for i in n:
			var lo2 := float(i) / n
			var hi2 := float(i + 1) / n
			if f >= hi2 - 0.0001:
				shares.append(color)
			elif f > lo2:
				shares.append(color.darkened(0.15))   # the partly lit block
			else:
				shares.append(Color(0.55, 0.67, 0.9, 0.2))
	for i in n:
		var a0 := start + i * step + gap / 2.0
		var a1 := start + (i + 1) * step - gap / 2.0
		draw_arc(c, r, a0, a1, 6, shares[i], thickness, false)
	# inner track with quarter ticks
	var ri := r - thickness / 2.0 - 4.0
	if ri > 8.0:
		draw_arc(c, ri, 0, TAU, 48, Color(Lcars.LINE2, 0.9), 1.0, true)
		for q in 4:
			var ang := start + q * PI / 2.0
			draw_line(c + Vector2(cos(ang), sin(ang)) * (ri - 3.0), c + Vector2(cos(ang), sin(ang)) * (ri + 1.5), Lcars.TEXT3, 1.5)
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
