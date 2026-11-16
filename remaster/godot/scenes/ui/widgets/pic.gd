class_name Pic
extends RefCounted
## Picture-led building blocks (16 Nov 2026 playtest: "original BOTF felt easier to navigate because of pictures"): an icon with a
## number beside it instead of a sentence, the colour of a sector or a structure family, and the tech-level lock. The words go to
## the tooltip. Icons are the UiIcon set; structures use ChoiceIcon (glyph of what the structure does on a tinted plate).

## The colour of a UiIcon family on screen: food green, industry the secondary accent, research blue, energy amber, defence cyan.
static func tint(icon_name: String) -> Color:
	match icon_name:
		"food": return Lcars.GOOD
		"industry": return Lcars.accent2()
		"research": return Lcars.INFO
		"energy": return Lcars.WARN
		"shield", "sensor": return Lcars.SHIELD
		"credits", "trade": return Color("#f2c46d")
		"morale": return Color("#d8c3ff")
		"ship": return Lcars.accent()
		"population", "growth": return Color("#9fd0ff")
		"attack", "intel": return Lcars.ALERT
	return Lcars.accent()

## [icon][number], one tooltip for both. `color` tints the icon (null: the family colour); the number is `num_color` or plain text.
static func num(icon_name: String, text: String, tip: String = "", num_color: Variant = null, px: float = 18.0, size: int = 15, color: Variant = null) -> HBoxContainer:
	var h := Lcars.hbox(4)
	h.mouse_filter = Control.MOUSE_FILTER_PASS
	h.tooltip_text = tip
	h.add_child(UiIcon.make(icon_name, px, tint(icon_name) if color == null else color))
	var l := Lcars.label(text, "Mono", size, Lcars.TEXT if num_color == null else num_color)
	l.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	h.add_child(l)
	return h

## A tooltip on any control (mouse passes so it shows).
static func tipped(c: Control, text: String) -> Control:
	c.tooltip_text = text
	if c.mouse_filter == Control.MOUSE_FILTER_IGNORE:
		c.mouse_filter = Control.MOUSE_FILTER_PASS
	return c

## The structure picture: its family glyph on a tinted plate, `px` square.
static func structure(name_: String, px: float = 40.0) -> ChoiceIcon:
	var ic := ChoiceIcon.new()
	ic.product = "structure"
	ic.label_name = name_
	ic.custom_minimum_size = Vector2(px, px)
	ic.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	ic.mouse_filter = Control.MOUSE_FILTER_IGNORE
	return ic

## "Requires Computers level 6" as [tech icon, "6"]; any other reason as ["lock", ""].
static func requirement(reason: String) -> Array:
	var r := reason.to_lower()
	if r.begins_with("requires ") and r.contains(" level "):
		var field := r.substr(9, r.find(" level ") - 9).strip_edges()
		var lvl := r.substr(r.find(" level ") + 7).strip_edges()
		for f in UiIcon.TECH_FIELDS:
			if f.trim_prefix("tech_") == field or (field == "biotech" and f == "tech_biotech"):
				return [f, lvl]
	return ["lock", ""]

## A faint plate for a picture tile: LCARS tab corners, accent line on the left.
static func tile_box(active: bool = false, tint_col: Variant = null) -> StyleBoxFlat:
	var c: Color = Lcars.accent() if tint_col == null else tint_col
	return Lcars.box_tab(Color(Lcars.PANEL2, 0.85), 14, 4, Color(c, 0.55) if active else Lcars.LINE2, 1, Vector4(8, 8, 8, 8))

## A toggle button that shows only an icon (name and explanation in the tooltip).
static func icon_toggle(icon_name: String, on: bool, on_toggle: Callable, tip: String, px: float = 22.0, box_px: Vector2 = Vector2(46, 38)) -> Button:
	var b := Lcars.toggle("", on, on_toggle, tip)
	b.custom_minimum_size = box_px
	var cc := CenterContainer.new()
	cc.set_anchors_preset(Control.PRESET_FULL_RECT)
	cc.mouse_filter = Control.MOUSE_FILTER_IGNORE
	cc.add_child(UiIcon.make(icon_name, px, Lcars.TEXT if on else Lcars.TEXT2))
	b.add_child(cc)
	return b

## A quiet plate with a plus: where something will go (an empty build-queue slot).
static func empty_slot(px: float = 42.0, tip: String = "") -> Control:
	var p := PanelContainer.new()
	p.add_theme_stylebox_override("panel", Lcars.box_tab(Color(Lcars.PANEL, 0.4), 12, 3, Lcars.LINE2, 1, Vector4(0, 0, 0, 0)))
	p.custom_minimum_size = Vector2(px, px)
	p.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	var cc := CenterContainer.new()
	cc.mouse_filter = Control.MOUSE_FILTER_IGNORE
	cc.add_child(UiIcon.make("plus", px * 0.4, Lcars.TEXT3))
	p.add_child(cc)
	return tipped(p, tip)
