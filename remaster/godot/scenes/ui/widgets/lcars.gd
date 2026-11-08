class_name Lcars
extends RefCounted
## LCARS modern look: palettes, fonts, the Theme resource and small widget builders shared by every screen.
## The design system of the widget layer (docs/CLIENT-ARCHITECTURE.md): tokens (colours, fonts), the Theme and the
## component builders every presentation script uses. Composite widgets live beside it in scenes/ui/widgets.

const BG := Color("#04060c")
const BG2 := Color("#070b16")
const PANEL := Color(0.039, 0.059, 0.118, 0.9)
const PANEL_SOLID := Color("#0a1022")
const PANEL2 := Color("#0e1630")
const PANEL3 := Color("#131d3c")
const LINE := Color(0.55, 0.67, 0.9, 0.13)
const LINE2 := Color(0.55, 0.67, 0.9, 0.26)
const TEXT := Color("#e7edf9")
const TEXT2 := Color("#b2bfd8")
const TEXT3 := Color("#7c8aa8")
const GOOD := Color("#5fd98f")
const WARN := Color("#f5c451")
const ALERT := Color("#ff5c50")
const INFO := Color("#7fb3ff")
const SHIELD := Color("#6fd3ff")

## Per-faction palettes (primary, secondary, tertiary, map owner colour, ink on fills).
const PALETTES := {
	0: {"key": "neutral", "primary": "#8fa6c8", "secondary": "#c3cfe0", "tertiary": "#5b6b86", "map": "#9aa6b8", "ink": "#05070d"},
	1: {"key": "federation", "primary": "#6fa8ff", "secondary": "#f2b45a", "tertiary": "#a8b8ff", "map": "#5b9dff", "ink": "#050912"},
	2: {"key": "klingon", "primary": "#e0484a", "secondary": "#c98a4b", "tertiary": "#b86b52", "map": "#e2463f", "ink": "#120505"},
	3: {"key": "romulan", "primary": "#3fc98a", "secondary": "#93dcbf", "tertiary": "#59a88a", "map": "#35c37f", "ink": "#03110a"},
	4: {"key": "cardassian", "primary": "#d9a44a", "secondary": "#e9d6a6", "tertiary": "#b88a55", "map": "#d6a13f", "ink": "#120c03"},
	5: {"key": "ferengi", "primary": "#f08a2c", "secondary": "#f5c16c", "tertiary": "#c8743d", "map": "#f28c28", "ink": "#130902"},
}
## Okabe-Ito owner colours for the colour-blind setting.
const COLORBLIND := {
	1: {"key": "federation", "primary": "#56b4e9", "secondary": "#e69f00", "tertiary": "#9fd3f2", "map": "#56b4e9", "ink": "#03101a"},
	2: {"key": "klingon", "primary": "#e0662a", "secondary": "#f0b27a", "tertiary": "#b3521f", "map": "#d55e00", "ink": "#140700"},
	3: {"key": "romulan", "primary": "#16b38a", "secondary": "#8fe0c8", "tertiary": "#0b7a5e", "map": "#009e73", "ink": "#00130d"},
	4: {"key": "cardassian", "primary": "#f0e442", "secondary": "#faf3a8", "tertiary": "#b8ad2a", "map": "#f0e442", "ink": "#141200"},
	5: {"key": "ferengi", "primary": "#d98cbb", "secondary": "#f0c6df", "tertiary": "#a8628e", "map": "#cc79a7", "ink": "#14060e"},
}
const FACTIONS := [
	{"id": 1, "short": "Federation", "capital": "Sol", "motto": "Explore. Unite. Endure.", "traits": ["Balanced economy", "Diplomatic reach", "Science vessels"]},
	{"id": 2, "short": "Klingon", "capital": "Qo'noS", "motto": "Glory is earned in fire.", "traits": ["Aggressive fleets", "Cloaking raiders", "Warrior culture"]},
	{"id": 3, "short": "Romulan", "capital": "Romulus", "motto": "Patience is the sharpest blade.", "traits": ["Cloaked warbirds", "Espionage", "Secretive state"]},
	{"id": 4, "short": "Cardassian", "capital": "Cardassia", "motto": "Order. Duty. The State.", "traits": ["Industrial might", "Security services", "Disciplined"]},
	{"id": 5, "short": "Ferengi", "capital": "Ferenginar", "motto": "Every contact is a customer.", "traits": ["Trade & credits", "Opportunists", "Fast traders"]},
]
const RESEARCH_BLURBS := [
	"Life sciences, terraforming and food production.",
	"Computing, sensors, research output and intelligence.",
	"Engineering, industry and hull construction.",
	"Power generation, shields and beam weapons.",
	"Warp drive, speed and operational range.",
	"Weapon systems, torpedoes and ground assault.",
]
const CATEGORY_LABELS := {
	"construction": "Construction", "research": "Research", "exploration": "Exploration", "diplomacy": "Diplomacy",
	"intel": "Intelligence", "combat": "Combat", "colonization": "Colonization", "events": "Galactic events",
	"stations": "Stations", "victory": "Victory", "economy": "Colonies & economy", "fleets": "Fleet operations",
}
## Emblem outlines in a 0..100 box: distinct silhouettes so owners read without colour.
const EMBLEMS := {
	0: [Vector2(50, 12), Vector2(88, 50), Vector2(50, 88), Vector2(12, 50)],
	1: [Vector2(50, 8), Vector2(78, 86), Vector2(50, 68), Vector2(22, 86)],
	2: [Vector2(50, 6), Vector2(60, 42), Vector2(94, 58), Vector2(58, 60), Vector2(50, 94), Vector2(42, 60), Vector2(6, 58), Vector2(40, 42)],
	3: [Vector2(50, 20), Vector2(62, 44), Vector2(96, 30), Vector2(72, 62), Vector2(56, 60), Vector2(50, 84), Vector2(44, 60), Vector2(28, 62), Vector2(4, 30), Vector2(38, 44)],
	4: [Vector2(10, 16), Vector2(90, 16), Vector2(78, 38), Vector2(70, 38), Vector2(50, 90), Vector2(30, 38), Vector2(22, 38)],
	5: [],
}

static var colorblind := false
static var accent_empire := 1
static var ui_scale := 1.0
static var _fonts := {}

static func palette(empire: int) -> Dictionary:
	if colorblind and COLORBLIND.has(empire):
		return COLORBLIND[empire]
	return PALETTES.get(empire, PALETTES[0])

static func owner_color(empire: int) -> Color:
	return Color(palette(empire)["map"])

static func accent() -> Color:
	return Color(palette(accent_empire)["primary"])

static func accent2() -> Color:
	return Color(palette(accent_empire)["secondary"])

static func accent3() -> Color:
	return Color(palette(accent_empire)["tertiary"])

static func ink() -> Color:
	return Color(palette(accent_empire)["ink"])

static func tone_color(tone: String) -> Color:
	match tone:
		"secondary": return accent2()
		"tertiary": return accent3()
		"good": return GOOD
		"warn": return WARN
		"alert": return ALERT
		"info": return INFO
		"ghost", "dim": return TEXT3
		"shield": return SHIELD
	return accent()

# ---------------------------------------------------------------------------------------------
# Fonts
# ---------------------------------------------------------------------------------------------
static func _base_font(file: String) -> FontFile:
	var key := "file:" + file
	if not _fonts.has(key):
		var f: FontFile = AppLoader.resource("res://scenes/ui/fonts/%s" % file)
		_fonts[key] = f
	return _fonts[key]

## "display" = Antonio (condensed LCARS headings), "ui" = Inter, "mono" = Share Tech Mono.
static func font(kind: String = "ui", weight: int = 400) -> Font:
	var key := "%s:%d" % [kind, weight]
	if _fonts.has(key):
		return _fonts[key]
	var file := "inter.woff2"
	if kind == "display":
		file = "antonio.woff2"
	elif kind == "mono":
		file = "sharetechmono.woff2"
	var base := _base_font(file)
	var result: Font = base
	if kind != "mono":
		var v := FontVariation.new()
		v.base_font = base
		v.variation_opentype = {"wght": weight}
		if kind == "display":
			v.spacing_glyph = 1
		result = v
	_fonts[key] = result
	return result

# ---------------------------------------------------------------------------------------------
# Theme resource
# ---------------------------------------------------------------------------------------------
static func box(bg: Color, radius: int = 12, border: Color = Color.TRANSPARENT, border_w: int = 0, pad: Vector4 = Vector4(10, 6, 10, 6)) -> StyleBoxFlat:
	var s := StyleBoxFlat.new()
	s.bg_color = bg
	s.set_corner_radius_all(radius)
	s.corner_detail = 8
	if border_w > 0:
		s.border_color = border
		s.set_border_width_all(border_w)
	s.content_margin_left = pad.x
	s.content_margin_top = pad.y
	s.content_margin_right = pad.z
	s.content_margin_bottom = pad.w
	s.anti_aliasing = true
	return s

## Builds the complete LCARS Theme for a faction accent. Saved as lcars_theme.tres by tools/build_theme.gd.
static func build_theme(empire: int = 1) -> Theme:
	accent_empire = empire
	var a := accent()
	var t := Theme.new()
	t.default_font = font("ui", 420)
	t.default_font_size = 15
	# Labels
	t.set_color("font_color", "Label", TEXT)
	t.set_constant("line_spacing", "Label", 2)
	t.set_type_variation("Heading", "Label")
	t.set_font("font", "Heading", font("display", 600))
	t.set_font_size("font_size", "Heading", 30)
	t.set_color("font_color", "Heading", a)
	t.set_type_variation("SubHeading", "Label")
	t.set_font("font", "SubHeading", font("display", 500))
	t.set_font_size("font_size", "SubHeading", 20)
	t.set_color("font_color", "SubHeading", TEXT2)
	t.set_type_variation("Caption", "Label")
	t.set_font("font", "Caption", font("display", 500))
	t.set_font_size("font_size", "Caption", 13)
	t.set_color("font_color", "Caption", TEXT3)
	t.set_type_variation("Mono", "Label")
	t.set_font("font", "Mono", font("mono"))
	t.set_font_size("font_size", "Mono", 14)
	t.set_color("font_color", "Mono", TEXT2)
	t.set_type_variation("Muted", "Label")
	t.set_color("font_color", "Muted", TEXT3)
	t.set_font_size("font_size", "Muted", 13)
	# Buttons: LCARS pills
	t.set_font("font", "Button", font("display", 560))
	t.set_font_size("font_size", "Button", 17)
	t.set_color("font_color", "Button", ink())
	t.set_color("font_hover_color", "Button", ink())
	t.set_color("font_pressed_color", "Button", ink())
	t.set_color("font_focus_color", "Button", ink())
	t.set_color("font_disabled_color", "Button", Color(TEXT3, 0.8))
	t.set_color("icon_normal_color", "Button", ink())
	var pad := Vector4(16, 5, 16, 5)
	t.set_stylebox("normal", "Button", box(a, 16, Color.TRANSPARENT, 0, pad))
	t.set_stylebox("hover", "Button", box(a.lightened(0.18), 16, Color.TRANSPARENT, 0, pad))
	t.set_stylebox("pressed", "Button", box(a.darkened(0.15), 16, Color.TRANSPARENT, 0, pad))
	t.set_stylebox("disabled", "Button", box(Color(PANEL3, 0.9), 16, LINE2, 1, pad))
	var focus := box(Color.TRANSPARENT, 17, Color(1, 1, 1, 0.8), 2, pad)
	focus.draw_center = false
	t.set_stylebox("focus", "Button", focus)
	# Ghost pill variation (toggles, secondary controls)
	t.set_type_variation("GhostButton", "Button")
	t.set_stylebox("normal", "GhostButton", box(Color(PANEL2, 0.8), 14, LINE2, 1, pad))
	t.set_stylebox("hover", "GhostButton", box(PANEL3, 14, a, 1, pad))
	t.set_stylebox("pressed", "GhostButton", box(Color(a, 0.35), 14, a, 1, pad))
	t.set_stylebox("hover_pressed", "GhostButton", box(Color(a, 0.45), 14, a, 1, pad))
	t.set_color("font_color", "GhostButton", TEXT2)
	t.set_color("font_hover_color", "GhostButton", TEXT)
	t.set_color("font_pressed_color", "GhostButton", TEXT)
	t.set_color("font_hover_pressed_color", "GhostButton", TEXT)
	t.set_color("font_focus_color", "GhostButton", TEXT)
	# Rail buttons: square-left LCARS blocks
	t.set_type_variation("RailButton", "Button")
	for state in ["normal", "hover", "pressed", "hover_pressed"]:
		var c := a.darkened(0.25) if state == "normal" else (a.lightened(0.15) if state == "hover" else a.lightened(0.3))
		var sb := box(c, 0, Color.TRANSPARENT, 0, Vector4(14, 8, 14, 8))
		sb.corner_radius_top_right = 18
		sb.corner_radius_bottom_right = 18
		t.set_stylebox(state, "RailButton", sb)
	t.set_font_size("font_size", "RailButton", 19)
	t.set_constant("h_separation", "Button", 8)
	# Panels
	t.set_stylebox("panel", "Panel", box(PANEL, 18, LINE, 1))
	t.set_stylebox("panel", "PanelContainer", box(PANEL, 18, LINE, 1, Vector4(16, 14, 16, 14)))
	t.set_type_variation("Card", "PanelContainer")
	t.set_stylebox("panel", "Card", box(Color(PANEL2, 0.92), 14, LINE2, 1, Vector4(12, 10, 12, 10)))
	t.set_type_variation("Inset", "PanelContainer")
	t.set_stylebox("panel", "Inset", box(Color(BG2, 0.85), 10, LINE, 1, Vector4(10, 8, 10, 8)))
	# Inputs
	for type in ["LineEdit", "SpinBox"]:
		t.set_stylebox("normal", type, box(BG2, 10, LINE2, 1, Vector4(10, 6, 10, 6)))
		t.set_stylebox("focus", type, box(BG2, 10, a, 2, Vector4(10, 6, 10, 6)))
		t.set_color("font_color", type, TEXT)
	t.set_font("font", "LineEdit", font("mono"))
	t.set_stylebox("normal", "OptionButton", box(PANEL2, 12, LINE2, 1, pad))
	t.set_stylebox("hover", "OptionButton", box(PANEL3, 12, a, 1, pad))
	t.set_stylebox("pressed", "OptionButton", box(PANEL3, 12, a, 1, pad))
	t.set_color("font_color", "OptionButton", TEXT)
	t.set_color("font_hover_color", "OptionButton", TEXT)
	t.set_color("font_pressed_color", "OptionButton", TEXT)
	t.set_color("font_focus_color", "OptionButton", TEXT)
	t.set_color("font_color", "CheckBox", TEXT2)
	t.set_color("font_hover_color", "CheckBox", TEXT)
	t.set_color("font_pressed_color", "CheckBox", TEXT)
	t.set_color("font_hover_pressed_color", "CheckBox", TEXT)
	t.set_color("font_focus_color", "CheckBox", TEXT)
	t.set_font("font", "CheckBox", font("ui", 450))
	t.set_font_size("font_size", "CheckBox", 14)
	var empty := StyleBoxEmpty.new()
	for st in ["normal", "hover", "pressed", "hover_pressed", "focus"]:
		t.set_stylebox(st, "CheckBox", empty)
	# Sliders and progress
	t.set_stylebox("slider", "HSlider", box(PANEL3, 4, Color.TRANSPARENT, 0, Vector4(0, 3, 0, 3)))
	t.set_stylebox("grabber_area", "HSlider", box(a.darkened(0.2), 4, Color.TRANSPARENT, 0, Vector4(0, 3, 0, 3)))
	t.set_stylebox("grabber_area_highlight", "HSlider", box(a, 4, Color.TRANSPARENT, 0, Vector4(0, 3, 0, 3)))
	t.set_stylebox("background", "ProgressBar", box(PANEL3, 4, Color.TRANSPARENT, 0, Vector4.ZERO))
	t.set_stylebox("fill", "ProgressBar", box(a, 4, Color.TRANSPARENT, 0, Vector4.ZERO))
	t.set_color("font_color", "ProgressBar", TEXT)
	# Popups, menus, tooltips
	t.set_stylebox("panel", "PopupPanel", box(Color(PANEL_SOLID, 0.97), 14, a, 1, Vector4(8, 8, 8, 8)))
	t.set_stylebox("panel", "PopupMenu", box(Color(PANEL_SOLID, 0.97), 12, a, 1, Vector4(6, 6, 6, 6)))
	t.set_stylebox("hover", "PopupMenu", box(Color(a, 0.3), 8))
	t.set_color("font_color", "PopupMenu", TEXT)
	t.set_color("font_disabled_color", "PopupMenu", TEXT3)
	t.set_stylebox("panel", "TooltipPanel", box(Color(PANEL_SOLID, 0.97), 10, a, 1, Vector4(12, 8, 12, 8)))
	t.set_color("font_color", "TooltipLabel", TEXT)
	t.set_font("font", "TooltipLabel", font("ui", 420))
	t.set_font_size("font_size", "TooltipLabel", 14)
	# Scroll bars
	t.set_stylebox("scroll", "VScrollBar", box(Color(0, 0, 0, 0.2), 4, Color.TRANSPARENT, 0, Vector4(3, 0, 3, 0)))
	t.set_stylebox("grabber", "VScrollBar", box(Color(a, 0.45), 4))
	t.set_stylebox("grabber_highlight", "VScrollBar", box(a, 4))
	t.set_stylebox("grabber_pressed", "VScrollBar", box(a, 4))
	t.set_stylebox("scroll", "HScrollBar", box(Color(0, 0, 0, 0.2), 4, Color.TRANSPARENT, 0, Vector4(0, 3, 0, 3)))
	t.set_stylebox("grabber", "HScrollBar", box(Color(a, 0.45), 4))
	# Tabs
	t.set_stylebox("tab_selected", "TabBar", box(a, 12, Color.TRANSPARENT, 0, Vector4(14, 4, 14, 4)))
	t.set_stylebox("tab_unselected", "TabBar", box(PANEL2, 12, LINE2, 1, Vector4(14, 4, 14, 4)))
	t.set_stylebox("tab_hovered", "TabBar", box(PANEL3, 12, a, 1, Vector4(14, 4, 14, 4)))
	t.set_color("font_selected_color", "TabBar", ink())
	t.set_color("font_unselected_color", "TabBar", TEXT2)
	t.set_color("font_hovered_color", "TabBar", TEXT)
	t.set_font("font", "TabBar", font("display", 560))
	t.set_font_size("font_size", "TabBar", 17)
	t.set_constant("h_separation", "TabBar", 6)
	t.set_color("font_default_color", "RichTextLabel", TEXT2)
	t.set_font("bold_font", "RichTextLabel", font("ui", 650))
	t.set_font("normal_font", "RichTextLabel", font("ui", 420))
	t.set_font("mono_font", "RichTextLabel", font("mono"))
	t.set_font_size("normal_font_size", "RichTextLabel", 14)
	t.set_font_size("bold_font_size", "RichTextLabel", 14)
	t.set_stylebox("separator", "HSeparator", box(LINE2, 0, Color.TRANSPARENT, 0, Vector4.ZERO))
	t.set_constant("separation", "HSeparator", 10)
	t.set_constant("separation", "VBoxContainer", 8)
	t.set_constant("separation", "HBoxContainer", 8)
	t.set_constant("h_separation", "GridContainer", 10)
	t.set_constant("v_separation", "GridContainer", 6)
	return t

# ---------------------------------------------------------------------------------------------
# Widget builders
# ---------------------------------------------------------------------------------------------
static func label(text: String, variation: String = "", size: int = 0, color: Variant = null) -> Label:
	var l := Label.new()
	l.text = text
	if variation != "":
		l.theme_type_variation = variation
	if size > 0:
		l.add_theme_font_size_override("font_size", size)
	if color != null:
		l.add_theme_color_override("font_color", color)
	return l

static func heading(text: String, size: int = 30) -> Label:
	var l := label(text.to_upper(), "Heading", size)
	return l

static func caption(text: String) -> Label:
	return label(text.to_upper(), "Caption")

static func para(text: String, color: Color = TEXT2, size: int = 14) -> Label:
	var l := label(text, "", size, color)
	l.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	l.custom_minimum_size.x = 60
	return l

## A pill button. tone: primary | secondary | tertiary | good | warn | alert | ghost.
## When `reason` is non-empty the pill is disabled and the reason becomes its tooltip.
static func pill(text: String, tone: String = "primary", on_press: Callable = Callable(), reason: String = "", hint: String = "", key: String = "") -> Button:
	var b := Button.new()
	b.text = text.to_upper() if tone != "ghost" else text
	b.focus_mode = Control.FOCUS_ALL
	if tone == "ghost":
		b.theme_type_variation = "GhostButton"
	elif tone != "primary":
		var c := tone_color(tone)
		var pad := Vector4(16, 5, 16, 5)
		b.add_theme_stylebox_override("normal", box(c, 16, Color.TRANSPARENT, 0, pad))
		b.add_theme_stylebox_override("hover", box(c.lightened(0.18), 16, Color.TRANSPARENT, 0, pad))
		b.add_theme_stylebox_override("pressed", box(c.darkened(0.15), 16, Color.TRANSPARENT, 0, pad))
	var tip := hint
	if key != "":
		tip = ("%s  [%s]" % [hint, key]) if hint != "" else "[%s]" % key
	if reason != "":
		b.disabled = true
		tip = "Unavailable: " + reason
	b.tooltip_text = tip
	if on_press.is_valid():
		b.pressed.connect(on_press)
	return b

static func toggle(text: String, on: bool, on_toggle: Callable, hint: String = "") -> Button:
	var b := Button.new()
	b.text = text
	b.toggle_mode = true
	b.button_pressed = on
	b.theme_type_variation = "GhostButton"
	b.add_theme_font_size_override("font_size", 15)
	b.tooltip_text = hint
	b.toggled.connect(on_toggle)
	return b

static func hbox(sep: int = 8) -> HBoxContainer:
	var h := HBoxContainer.new()
	h.add_theme_constant_override("separation", sep)
	return h

static func vbox(sep: int = 8) -> VBoxContainer:
	var v := VBoxContainer.new()
	v.add_theme_constant_override("separation", sep)
	return v

static func spacer() -> Control:
	var c := Control.new()
	c.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	return c

static func grid(columns: int) -> GridContainer:
	var g := GridContainer.new()
	g.columns = columns
	return g

static func scroll(child: Control) -> ScrollContainer:
	var s := ScrollContainer.new()
	s.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	s.size_flags_vertical = Control.SIZE_EXPAND_FILL
	s.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	child.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	s.add_child(child)
	return s

## An LCARS framed panel: coloured header band with title, body container returned via frame.get_meta("body").
static func frame(title: String, sub: String = "", tone: String = "primary") -> PanelContainer:
	var p := PanelContainer.new()
	var outer := vbox(8)
	p.add_child(outer)
	var head := hbox(10)
	var band := ColorRect.new()
	band.color = tone_color(tone)
	band.custom_minimum_size = Vector2(26, 22)
	head.add_child(band)
	var t := label(title.to_upper(), "", 21, tone_color(tone))
	t.add_theme_font_override("font", font("display", 600))
	head.add_child(t)
	if sub != "":
		var s := label(sub.to_upper(), "Caption")
		s.size_flags_vertical = Control.SIZE_SHRINK_END
		head.add_child(s)
	var line := ColorRect.new()
	line.color = Color(tone_color(tone), 0.35)
	line.custom_minimum_size = Vector2(0, 2)
	line.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	line.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	head.add_child(line)
	outer.add_child(head)
	var body := vbox(8)
	body.size_flags_vertical = Control.SIZE_EXPAND_FILL
	outer.add_child(body)
	p.set_meta("body", body)
	return p

static func body(frame_panel: Control) -> VBoxContainer:
	return frame_panel.get_meta("body")

static func card() -> PanelContainer:
	var p := PanelContainer.new()
	p.theme_type_variation = "Card"
	return p

## Horizontal meter bar drawn as two rounded boxes.
static func meter(value: float, max_value: float, color: Color = Color(0, 0, 0, 0), height: int = 8, width: int = 0) -> ProgressBar:
	var bar := ProgressBar.new()
	bar.max_value = max(1.0, max_value)
	bar.value = clamp(value, 0.0, bar.max_value)
	bar.show_percentage = false
	bar.custom_minimum_size = Vector2(width, height)
	bar.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	bar.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	if color.a > 0:
		bar.add_theme_stylebox_override("fill", box(color, 4, Color.TRANSPARENT, 0, Vector4.ZERO))
	return bar

## A stat tile: small caption over a large value.
static func stat(caption_text: String, value: Variant, tone: String = "", tip: String = "") -> Control:
	var v := vbox(0)
	v.add_child(caption(caption_text))
	var val := label(num(value), "", 24, tone_color(tone) if tone != "" else TEXT)
	val.add_theme_font_override("font", font("display", 560))
	v.add_child(val)
	v.tooltip_text = tip
	v.mouse_filter = Control.MOUSE_FILTER_PASS
	return v

static func badge(text: String, tone: String = "primary") -> PanelContainer:
	var p := PanelContainer.new()
	var c := tone_color(tone)
	p.add_theme_stylebox_override("panel", box(Color(c, 0.18), 9, Color(c, 0.7), 1, Vector4(8, 1, 8, 1)))
	var l := label(text.to_upper(), "", 12, c)
	l.add_theme_font_override("font", font("display", 600))
	p.add_child(l)
	p.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	return p

static func separator() -> HSeparator:
	return HSeparator.new()

static func empty_note(text: String) -> Label:
	var l := para(text, TEXT3, 14)
	l.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	return l

## Formats JSON numbers (which arrive as floats) without a spurious ".0".
static func num(value: Variant) -> String:
	return Fmt.num(value)

static func signed(n: float) -> String:
	return Fmt.signed(n)

static func fmt(n: float) -> String:
	return Fmt.fmt(n)

## Draws a faction emblem into a CanvasItem at centre c with size s.
static func draw_emblem(ci: CanvasItem, empire: int, c: Vector2, s: float, color: Color, outline: bool = false) -> void:
	var pts: Array = EMBLEMS.get(empire, EMBLEMS[0])
	if empire == 5 or pts.is_empty():
		ci.draw_arc(c, s * 0.36, 0, TAU, 32, color, s * 0.16, true)
		return
	var poly := PackedVector2Array()
	for p in pts:
		poly.append(c + (p - Vector2(50, 50)) * s / 100.0)
	if outline:
		poly.append(poly[0])
		ci.draw_polyline(poly, color, 1.5, true)
	else:
		ci.draw_colored_polygon(poly, color)

static func emblem(empire: int, size: float = 28.0, color: Variant = null) -> Control:
	var e := EmblemView.new()
	e.empire = empire
	e.custom_minimum_size = Vector2(size, size)
	e.tint = owner_color(empire) if color == null else color
	return e

class EmblemView extends Control:
	var empire := 0
	var tint := Color.WHITE
	func _ready() -> void:
		mouse_filter = Control.MOUSE_FILTER_IGNORE
	func _draw() -> void:
		Lcars.draw_emblem(self, empire, size / 2.0, min(size.x, size.y), tint)

# ---------------------------------------------------------------------------------------------
# Layout components shared by the screens
# ---------------------------------------------------------------------------------------------
## A full-rect MarginContainer added to `host`; returns it. `sides` is margin per side (left, right, top, bottom).
static func screen_margin(host: Control, sides: Vector4 = Vector4(16, 16, 16, 16)) -> MarginContainer:
	var m := MarginContainer.new()
	m.set_anchors_preset(Control.PRESET_FULL_RECT)
	m.add_theme_constant_override("margin_left", int(sides.x))
	m.add_theme_constant_override("margin_right", int(sides.y))
	m.add_theme_constant_override("margin_top", int(sides.z))
	m.add_theme_constant_override("margin_bottom", int(sides.w))
	host.add_child(m)
	return m

## Screen title block: heading over a caption.
static func title_block(title: String, sub: String, size: int = 40) -> VBoxContainer:
	var t := vbox(0)
	t.add_child(heading(title, size))
	t.add_child(caption(sub))
	return t

## A selectable list row: a ghost button, highlighted when active. Fill it through row_inner().
static func row_button(active: bool, height: int = 52, radius: int = 12) -> Button:
	var btn := Button.new()
	btn.theme_type_variation = "GhostButton"
	btn.custom_minimum_size.y = height
	if active:
		btn.add_theme_stylebox_override("normal", box(Color(accent(), 0.2), radius, accent(), 1))
	return btn

## The content row inside a row_button.
static func row_inner(btn: Button, pad: int = 10, sep: int = 10) -> HBoxContainer:
	var h := hbox(sep)
	h.set_anchors_preset(Control.PRESET_FULL_RECT)
	h.offset_left = pad
	h.offset_right = -pad
	btn.add_child(h)
	return h

## Lets clicks pass through `n` and all its children to the button underneath.
static func ignore_mouse(n: Control) -> void:
	n.mouse_filter = Control.MOUSE_FILTER_IGNORE
	for c in n.find_children("*", "Control", true, false):
		c.mouse_filter = Control.MOUSE_FILTER_IGNORE

## A flat glyph button with a disabled reason.
static func icon_button(text: String, hint: String, reason: String, on: Callable) -> Button:
	var b := Button.new()
	b.text = text
	b.flat = true
	b.tooltip_text = ("Unavailable: " + reason) if reason != "" else hint
	b.disabled = reason != ""
	b.add_theme_color_override("font_color", TEXT2)
	b.add_theme_color_override("font_hover_color", accent())
	b.pressed.connect(on)
	return b

## A labelled bar: caption, meter, "v/max".
static func bar_row(cap: String, value: int, max_value: int, color: Color, cap_width: int = 60) -> HBoxContainer:
	var r := hbox(8)
	var l := label(cap, "Muted", 13)
	l.custom_minimum_size.x = cap_width
	r.add_child(l)
	r.add_child(meter(value, max_value, color, 8))
	r.add_child(label("%d/%d" % [value, max_value], "Mono", 13))
	return r

## Toggle bar: [[id, label]]; `current` is highlighted; on_pick(id) when one is chosen. Buttons are named prefix+id.
static func tab_bar(tabs: Array, current: String, on_pick: Callable, prefix: String = "Tab_") -> HBoxContainer:
	var row := hbox(6)
	for t in tabs:
		var id: String = t[0]
		var tg := toggle(t[1], current == id, func(_on): on_pick.call(id))
		tg.name = prefix + id
		row.add_child(tg)
	return row

## A stat over a tooltip; mouse passes so the tip shows.
static func tip(c: Control, text: String) -> Control:
	c.tooltip_text = text
	c.mouse_filter = Control.MOUSE_FILTER_PASS
	return c
