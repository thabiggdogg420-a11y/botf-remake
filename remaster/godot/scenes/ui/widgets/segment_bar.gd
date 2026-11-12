class_name SegmentBar
extends Control
## A stacked bar of parts with a legend (docs/UI-DESIGN-SYSTEM.md §5): parts is [[label, value, Color]]. Negative
## parts draw in a separate red tail. Hover shows each part.

var parts: Array = []
var ex := {}
var legend := true

static func make(parts_: Array, explain: Variant = "", show_legend: bool = true, height: float = 12.0) -> SegmentBar:
	var s := SegmentBar.new()
	s.parts = parts_
	s.legend = show_legend
	s.custom_minimum_size = Vector2(120, height + (20.0 if show_legend else 0.0))
	s.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var e := RichTip.from(explain)
	if not e.has("parts"):
		e["parts"] = parts_.map(func(p): return [p[0], Fmt.num(p[1])])
	s.ex = e
	s.tooltip_text = RichTip.text(e)
	s.mouse_filter = Control.MOUSE_FILTER_PASS
	return s

func _make_custom_tooltip(_for_text: String) -> Object:
	return RichTip.build(ex)

func _draw() -> void:
	var h := size.y - (20.0 if legend else 0.0)
	var pos := 0.0
	var neg := 0.0
	for p in parts:
		var v := float(p[1])
		if v >= 0: pos += v
		else: neg -= v
	var tot: float = max(1.0, pos + neg)
	draw_rect(Rect2(0, 0, size.x, h), Lcars.PANEL3)
	var x := 0.0
	for p in parts:
		var v := float(p[1])
		if v <= 0:
			continue
		var w := size.x * v / tot
		draw_rect(Rect2(x, 0, max(0.0, w - 2.0), h), p[2])
		x += w
	for p in parts:
		var v := float(p[1])
		if v >= 0:
			continue
		var w := size.x * -v / tot
		draw_rect(Rect2(x, 0, max(0.0, w - 2.0), h), Color(Lcars.ALERT, 0.55))
		for k in range(int(x), int(x + w), 6):
			draw_line(Vector2(k, h), Vector2(k + h, 0), Color(Lcars.ALERT, 0.8), 1.0)
		x += w
	if legend:
		var fnt := Lcars.font("ui", 420)
		var lx := 0.0
		for p in parts:
			draw_rect(Rect2(lx + 1, h + 8, 8, 8), p[2] if float(p[1]) >= 0 else Lcars.ALERT)
			var t := "%s %s" % [p[0], Fmt.num(p[1])]
			draw_string(fnt, Vector2(lx + 13, h + 17), t, HORIZONTAL_ALIGNMENT_LEFT, -1, 12, Lcars.TEXT2)
			lx += 22 + fnt.get_string_size(t, HORIZONTAL_ALIGNMENT_LEFT, -1, 12).x
			if lx > size.x - 40:
				break
