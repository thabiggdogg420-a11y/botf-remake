class_name RichTip
extends RefCounted
## The number-explanation tooltip (docs/UI-DESIGN-SYSTEM.md §8). An explanation is a plain Dictionary built by a view
## model: {title, value, parts: [[label, value, tone]], effect, forecast}. `text` is the plain-text form (it is also
## the control's tooltip_text, so tests and the stock tooltip see the same content); `build` is the rich panel.

static func text(ex: Dictionary) -> String:
	var lines := []
	var head := str(ex.get("title", ""))
	if ex.has("value") and str(ex["value"]) != "":
		head += ("  " if head != "" else "") + str(ex["value"])
	if head != "":
		lines.append(head)
	for p in ex.get("parts", []):
		lines.append("  %s: %s" % [p[0], p[1]])
	if str(ex.get("effect", "")) != "":
		lines.append(str(ex["effect"]))
	if str(ex.get("forecast", "")) != "":
		lines.append("Forecast: " + str(ex["forecast"]))
	return "\n".join(lines)

## A plain tooltip string becomes an explanation with only a title line.
static func from(v: Variant) -> Dictionary:
	if v is Dictionary:
		return v
	return {"title": str(v)}

static func build(ex: Dictionary) -> Control:
	var col := Lcars.vbox(4)
	col.custom_minimum_size.x = 260
	var head := Lcars.hbox(10)
	var t := Lcars.label(str(ex.get("title", "")).to_upper(), "", 15, Lcars.accent())
	t.add_theme_font_override("font", Lcars.font("display", 600))
	head.add_child(t)
	if ex.has("value") and str(ex["value"]) != "":
		head.add_child(Lcars.spacer())
		var v := Lcars.label(str(ex["value"]), "", 18, Lcars.TEXT)
		v.add_theme_font_override("font", Lcars.font("display", 560))
		head.add_child(v)
	col.add_child(head)
	var parts: Array = ex.get("parts", [])
	if parts.size() > 0:
		var line := ColorRect.new()
		line.color = Lcars.LINE2
		line.custom_minimum_size = Vector2(0, 1)
		col.add_child(line)
		for p in parts:
			var r := Lcars.hbox(16)
			var l := Lcars.label(str(p[0]), "", 13, Lcars.TEXT2)
			l.size_flags_horizontal = Control.SIZE_EXPAND_FILL
			r.add_child(l)
			var tone: String = p[2] if p.size() > 2 else ""
			r.add_child(Lcars.label(str(p[1]), "Mono", 13, Lcars.tone_color(tone) if tone != "" else Lcars.TEXT))
			col.add_child(r)
	if str(ex.get("effect", "")) != "":
		var e := Lcars.para(str(ex["effect"]), Lcars.TEXT2, 13)
		e.custom_minimum_size.x = 260
		col.add_child(e)
	if str(ex.get("forecast", "")) != "":
		var fr := Lcars.hbox(6)
		fr.add_child(UiIcon.make("clock", 14, Lcars.accent2()))
		var f := Lcars.para(str(ex["forecast"]), Lcars.accent2(), 13)
		f.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		fr.add_child(f)
		col.add_child(fr)
	var m := MarginContainer.new()
	for side in ["left", "right", "top", "bottom"]:
		m.add_theme_constant_override("margin_" + side, 4)
	m.add_child(col)
	return m

## A container whose tooltip is the rich explanation of `ex`.
class Explained extends MarginContainer:
	var ex := {}
	func _init(e: Dictionary = {}) -> void:
		ex = e
		tooltip_text = RichTip.text(e)
		mouse_filter = Control.MOUSE_FILTER_PASS
		focus_mode = Control.FOCUS_NONE
	func _make_custom_tooltip(_for_text: String) -> Object:
		return RichTip.build(ex)
