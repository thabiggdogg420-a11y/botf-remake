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
	6: {"key": "dominion", "primary": "#8f6ad8", "secondary": "#c7b3f0", "tertiary": "#6f55a8", "map": "#8a5fd6", "ink": "#0a0614"},
	7: {"key": "borg", "primary": "#5cd45a", "secondary": "#a8e8a4", "tertiary": "#3f9a3e", "map": "#4fcf4c", "ink": "#040e04"},
	8: {"key": "breen", "primary": "#7fd3e0", "secondary": "#c2edf3", "tertiary": "#559fab", "map": "#6ccadb", "ink": "#041013"},
	9: {"key": "threat", "primary": "#ff5a4a", "secondary": "#ffb09a", "tertiary": "#b8483c", "map": "#ff5a4a", "ink": "#140504"}, # threat actors (schema 20): never a campaign power
}
## Okabe-Ito owner colours for the colour-blind setting.
const COLORBLIND := {
	1: {"key": "federation", "primary": "#56b4e9", "secondary": "#e69f00", "tertiary": "#9fd3f2", "map": "#56b4e9", "ink": "#03101a"},
	2: {"key": "klingon", "primary": "#e0662a", "secondary": "#f0b27a", "tertiary": "#b3521f", "map": "#d55e00", "ink": "#140700"},
	3: {"key": "romulan", "primary": "#16b38a", "secondary": "#8fe0c8", "tertiary": "#0b7a5e", "map": "#009e73", "ink": "#00130d"},
	4: {"key": "cardassian", "primary": "#f0e442", "secondary": "#faf3a8", "tertiary": "#b8ad2a", "map": "#f0e442", "ink": "#141200"},
	5: {"key": "ferengi", "primary": "#d98cbb", "secondary": "#f0c6df", "tertiary": "#a8628e", "map": "#cc79a7", "ink": "#14060e"},
	6: {"key": "dominion", "primary": "#b9b9ff", "secondary": "#e0e0ff", "tertiary": "#8c8cc8", "map": "#a6a6ff", "ink": "#0a0a18"},
	7: {"key": "borg", "primary": "#8fe28a", "secondary": "#c9f2c6", "tertiary": "#5fae5a", "map": "#7cd977", "ink": "#04100a"},
	8: {"key": "breen", "primary": "#4fb0c8", "secondary": "#a4dae6", "tertiary": "#37808f", "map": "#3fa2bd", "ink": "#03121a"},
}
const FACTIONS := [
	{"id": 1, "short": "Federation", "capital": "Sol", "motto": "Explore. Unite. Endure.", "traits": ["Balanced economy", "Diplomatic reach", "Science vessels"]},
	{"id": 2, "short": "Klingon", "capital": "Qo'noS", "motto": "Glory is earned in fire.", "traits": ["Aggressive fleets", "Cloaking raiders", "Warrior culture"]},
	{"id": 3, "short": "Romulan", "capital": "Romulus", "motto": "Patience is the sharpest blade.", "traits": ["Cloaked warbirds", "Espionage", "Secretive state"]},
	{"id": 4, "short": "Cardassian", "capital": "Cardassia", "motto": "Order. Duty. The State.", "traits": ["Industrial might", "Security services", "Disciplined"]},
	{"id": 5, "short": "Ferengi", "capital": "Ferenginar", "motto": "Every contact is a customer.", "traits": ["Trade & credits", "Opportunists", "Fast traders"]},
	{"id": 6, "short": "Dominion", "key": "dominion", "capital": "Omarion Nebula", "motto": "Order is the only peace.", "traits": ["Founders and Vorta", "Jem'Hadar shock troops", "Absolute order"]},
	{"id": 7, "short": "Borg", "key": "borg", "capital": "Unimatrix 001", "motto": "Resistance is futile.", "traits": ["Collective will", "Adaptive weapons", "Assimilation"]},
	{"id": 8, "short": "Breen", "key": "breen", "capital": "Breen", "motto": "Silence keeps the confederacy.", "traits": ["Energy dampeners", "Secretive confederacy", "Cold-world raiders"]},
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
## Emblems are drawn by Insignia (insignia.gd): one vector mark per empire and power, with a heavier small variant.

static var colorblind := false
## Schema 25: the active pack's recolouring and renaming of the empires (the Mirror Universe), set from the core's game view by
## set_pack_look. Empty for every other game, so palette() and faction_short() then answer from the tables above.
static var _pack_palette := {}
static var _pack_names := {}
static var accent_empire := 1
static var ui_scale := 1.0
static var _fonts := {}

static func palette(empire: int) -> Dictionary:
	if colorblind and COLORBLIND.has(empire):
		return COLORBLIND[empire]
	if _pack_palette.has(empire):
		return _pack_palette[empire]
	return PALETTES.get(empire, PALETTES[0])

## `palette` is the view's meta.palette ({"1": {"color": "#9b1c1c", "accent": "#e0b030"}}), `names` its meta.empireNames ({"1": "Terran Empire"}).
static func set_pack_look(pal: Dictionary, names: Dictionary) -> void:
	_pack_palette = {}
	for k in pal:
		var id := int(k)
		var base: Dictionary = PALETTES.get(id, PALETTES[0]).duplicate()
		var c := Color(str(pal[k]["color"]))
		var a := Color(str(pal[k]["accent"]))
		base["primary"] = c.lightened(0.18).to_html(false)
		base["map"] = c.lightened(0.1).to_html(false)
		base["secondary"] = a.to_html(false)
		base["tertiary"] = c.darkened(0.2).lightened(0.25).to_html(false)
		_pack_palette[id] = base
	_pack_names = {}
	for k in names:
		_pack_names[int(k)] = str(names[k])

## The empire's short name: the pack's own (Terran Empire), else the table's.
static func faction_short(empire: int) -> String:
	if _pack_names.has(empire):
		return str(_pack_names[empire])
	return str(FACTIONS[clampi(empire, 1, FACTIONS.size()) - 1]["short"])

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
## Bundled symbol fonts: the UI text uses ● ◇ ▸ ▲ ▼ ✓ ✗ ✕ ⚡ → ∞ and the like, which Antonio, Inter and Share Tech Mono lack.
## Desktop finds them in a system font; the web build has no system fonts, so they showed as missing-glyph boxes. Every base
## font lists these as fallbacks (and system fallback stays on), so the same glyphs draw in both builds.
const SYMBOL_FONTS := ["symbols.ttf", "symbols-math.ttf"]

static func _base_font(file: String) -> FontFile:
	var key := "file:" + file
	if not _fonts.has(key):
		var f: FontFile = AppLoader.resource("res://scenes/ui/fonts/%s" % file)
		if f != null:
			var fb: Array[Font] = []
			for sf in SYMBOL_FONTS:
				var sym := AppLoader.resource("res://scenes/ui/fonts/%s" % sf) as Font
				if sym != null:
					fb.append(sym)
			f.fallbacks = fb
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

## Drop-in for box(): the LCARS sweep, a big radius on the left and a quarter of it on the right.
static func box_lcars(bg: Color, radius: int = 12, border: Color = Color.TRANSPARENT, border_w: int = 0, pad: Vector4 = Vector4(10, 6, 10, 6)) -> StyleBoxFlat:
	return box_tab(bg, radius, maxi(2, radius / 4), border, border_w, pad)

## An LCARS tab: the left end rounded to `r_left`, the right end nearly square (`r_right`), the Okudagram way round.
static func box_tab(bg: Color, r_left: int = 12, r_right: int = 3, border: Color = Color.TRANSPARENT, border_w: int = 0, pad: Vector4 = Vector4(10, 6, 10, 6)) -> StyleBoxFlat:
	var s := box(bg, r_left, border, border_w, pad)
	s.corner_radius_top_right = r_right
	s.corner_radius_bottom_right = r_right
	return s

## The era's panel: tinted surface, a hairline border and a lit top edge in the accent. The sweep is the LCARS one (a big radius at
## the top left, small elsewhere) and there is no web-style drop shadow: LCARS is flat colour on black (lcars-theme DESIGN.md).
static func panel_box(edge: Color = Color(0, 0, 0, 0)) -> StyleBoxFlat:
	var rr := UiTokens.radius()
	var sb := box(Color(UiTokens.panel(), 0.9), maxi(3, rr / 3), LINE, 1, Vector4(16, 14, 16, 14))
	sb.corner_radius_top_left = rr
	if edge.a > 0:
		sb.border_color = Color(edge, 0.22)
		sb.border_width_top = 2
	return sb

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
	var pr := UiTokens.pill_radius()
	var rr := UiTokens.radius()
	var pnl := UiTokens.panel()
	t.set_stylebox("normal", "Button", box(a, pr, Color.TRANSPARENT, 0, pad))
	t.set_stylebox("hover", "Button", box(a.lightened(0.18), pr, Color.TRANSPARENT, 0, pad))
	t.set_stylebox("pressed", "Button", box(a.darkened(0.15), pr, Color.TRANSPARENT, 0, pad))
	t.set_stylebox("disabled", "Button", box(Color(PANEL3, 0.9), pr, LINE2, 1, pad))
	var focus := box(Color.TRANSPARENT, pr + 1, Color(1, 1, 1, 0.8), 2, pad)
	focus.draw_center = false
	t.set_stylebox("focus", "Button", focus)
	# Ghost pill variation (toggles, secondary controls)
	t.set_type_variation("GhostButton", "Button")
	# LCARS tabs: the left end round, the right end square; the lit one is a filled tab, not a web-style outline
	var gl := maxi(3, pr - 4)
	t.set_stylebox("normal", "GhostButton", box_tab(Color(a, 0.12), gl, 2, Color.TRANSPARENT, 0, pad))
	t.set_stylebox("hover", "GhostButton", box_tab(Color(a, 0.26), gl, 2, Color.TRANSPARENT, 0, pad))
	t.set_stylebox("pressed", "GhostButton", box_tab(Color(a, 0.55), gl, 2, Color.TRANSPARENT, 0, pad))
	t.set_stylebox("hover_pressed", "GhostButton", box_tab(Color(a, 0.65), gl, 2, Color.TRANSPARENT, 0, pad))
	t.set_color("font_color", "GhostButton", TEXT2)
	t.set_color("font_hover_color", "GhostButton", TEXT)
	t.set_color("font_pressed_color", "GhostButton", ink())
	t.set_color("font_hover_pressed_color", "GhostButton", ink())
	t.set_color("font_focus_color", "GhostButton", TEXT)
	# Rail buttons: square-left LCARS blocks
	t.set_type_variation("RailButton", "Button")
	for state in ["normal", "hover", "pressed", "hover_pressed"]:
		var c := a.darkened(0.25) if state == "normal" else (a.lightened(0.15) if state == "hover" else a.lightened(0.3))
		var sb := box(c, 0, Color.TRANSPARENT, 0, Vector4(14, 8, 14, 8))
		sb.corner_radius_top_right = pr
		sb.corner_radius_bottom_right = pr
		t.set_stylebox(state, "RailButton", sb)
	t.set_font_size("font_size", "RailButton", 19)
	t.set_constant("h_separation", "Button", 8)
	# Panels
	t.set_stylebox("panel", "Panel", box(Color(pnl, 0.9), rr, LINE, 1))
	t.set_stylebox("panel", "PanelContainer", panel_box(a))
	t.set_type_variation("Card", "PanelContainer")
	t.set_stylebox("panel", "Card", box_tab(Color(PANEL2, 0.92), maxi(4, rr - 2), 3, LINE2, 1, Vector4(12, 10, 12, 10)))
	t.set_type_variation("Inset", "PanelContainer")
	t.set_stylebox("panel", "Inset", box(Color(BG2, 0.85), maxi(4, rr - 4), LINE, 1, Vector4(10, 8, 10, 8)))
	# Inputs
	for type in ["LineEdit", "SpinBox"]:
		t.set_stylebox("normal", type, box(BG2, 10, LINE2, 1, Vector4(10, 6, 10, 6)))
		t.set_stylebox("focus", type, box(BG2, 10, a, 2, Vector4(10, 6, 10, 6)))
		t.set_color("font_color", type, TEXT)
	t.set_font("font", "LineEdit", font("mono"))
	t.set_stylebox("normal", "OptionButton", box_tab(PANEL2, pr, 3, LINE2, 1, pad))
	t.set_stylebox("hover", "OptionButton", box_tab(PANEL3, pr, 3, a, 1, pad))
	t.set_stylebox("pressed", "OptionButton", box_tab(PANEL3, pr, 3, a, 1, pad))
	t.set_icon("arrow", "OptionButton", delta_texture(a, 14, PI))
	t.set_constant("arrow_margin", "OptionButton", 8)
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
	# Switches and check lamps in LCARS blocks instead of the engine's rounded web toggles
	for ty in ["CheckButton", "CheckBox"]:
		var on_tex := switch_texture(a, true) if ty == "CheckButton" else lamp_texture(a, true)
		var off_tex := switch_texture(a, false) if ty == "CheckButton" else lamp_texture(a, false)
		for nm in ["checked", "checked_mirrored"]:
			t.set_icon(nm, ty, on_tex)
		for nm in ["unchecked", "unchecked_mirrored"]:
			t.set_icon(nm, ty, off_tex)
		t.set_icon("checked_disabled", ty, switch_texture(TEXT3, true) if ty == "CheckButton" else lamp_texture(TEXT3, true))
		t.set_icon("unchecked_disabled", ty, switch_texture(TEXT3, false) if ty == "CheckButton" else lamp_texture(TEXT3, false))
		t.set_icon("radio_checked", ty, lamp_texture(a, true))
		t.set_icon("radio_unchecked", ty, lamp_texture(a, false))
	t.set_icon("updown", "SpinBox", updown_texture(a))
	t.set_icon("grabber", "HSlider", slider_knob_texture(a.darkened(0.1)))
	t.set_icon("grabber_highlight", "HSlider", slider_knob_texture(a.lightened(0.25)))
	t.set_icon("grabber_disabled", "HSlider", slider_knob_texture(TEXT3))
	# Sliders and progress
	t.set_stylebox("slider", "HSlider", box_tab(PANEL3, 4, 1, Color.TRANSPARENT, 0, Vector4(0, 3, 0, 3)))
	t.set_stylebox("grabber_area", "HSlider", box_tab(a.darkened(0.2), 4, 1, Color.TRANSPARENT, 0, Vector4(0, 3, 0, 3)))
	t.set_stylebox("grabber_area_highlight", "HSlider", box_tab(a, 4, 1, Color.TRANSPARENT, 0, Vector4(0, 3, 0, 3)))
	t.set_stylebox("background", "ProgressBar", StyleBoxEmpty.new())   # LcarsMeter draws its own segmented bar
	t.set_stylebox("fill", "ProgressBar", StyleBoxEmpty.new())
	t.set_color("font_color", "ProgressBar", TEXT)
	# Popups, menus, tooltips
	t.set_stylebox("panel", "PopupPanel", box_tab(Color(PANEL_SOLID, 0.97), rr, 3, a, 1, Vector4(8, 8, 8, 8)))
	t.set_stylebox("panel", "PopupMenu", box_tab(Color(PANEL_SOLID, 0.97), rr, 3, a, 1, Vector4(6, 6, 6, 6)))
	t.set_stylebox("hover", "PopupMenu", box_tab(Color(a, 0.3), 8, 2))
	t.set_color("font_color", "PopupMenu", TEXT)
	t.set_color("font_disabled_color", "PopupMenu", TEXT3)
	t.set_stylebox("panel", "TooltipPanel", box_tab(Color(PANEL_SOLID, 0.97), maxi(6, rr - 2), 3, a, 1, Vector4(12, 8, 12, 8)))
	t.set_color("font_color", "TooltipLabel", TEXT)
	t.set_font("font", "TooltipLabel", font("ui", 420))
	t.set_font_size("font_size", "TooltipLabel", 14)
	# Scroll bars
	t.set_stylebox("scroll", "VScrollBar", box(Color(0, 0, 0, 0.2), 1, Color.TRANSPARENT, 0, Vector4(3, 0, 3, 0)))
	t.set_stylebox("grabber", "VScrollBar", box_tab(Color(a, 0.45), 1, 1))
	t.set_stylebox("grabber_highlight", "VScrollBar", box_tab(a, 1, 1))
	t.set_stylebox("grabber_pressed", "VScrollBar", box_tab(a, 1, 1))
	t.set_stylebox("scroll", "HScrollBar", box(Color(0, 0, 0, 0.2), 1, Color.TRANSPARENT, 0, Vector4(0, 3, 0, 3)))
	t.set_stylebox("grabber", "HScrollBar", box_tab(Color(a, 0.45), 1, 1))
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
		var pr := UiTokens.pill_radius()
		b.add_theme_stylebox_override("normal", box(c, pr, Color.TRANSPARENT, 0, pad))
		b.add_theme_stylebox_override("hover", box(c.lightened(0.18), pr, Color.TRANSPARENT, 0, pad))
		b.add_theme_stylebox_override("pressed", box(c.darkened(0.15), pr, Color.TRANSPARENT, 0, pad))
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

## The Back control of the bare screens: a tertiary pill with the delta pointing back, not a plain dark box.
static func back_pill(on_press: Callable) -> Button:
	var b := pill("    Back", "tertiary", on_press, "", "", "Esc")
	b.name = "Back"
	var ic := UiIcon.make("delta_left", 16, ink())
	ic.position = Vector2(14, 9)
	b.add_child(ic)
	b.custom_minimum_size.y = 40
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

## An LCARS framed panel: era header (docs/UI-DESIGN-SYSTEM.md §5) with title, optional icon and sub caption; the
## body container is returned via frame.get_meta("body"), the header row via get_meta("head").
static func frame(title: String, sub: String = "", tone: String = "primary", icon_name: String = "") -> PanelContainer:
	var p := PanelContainer.new()
	var tc := tone_color(tone)
	p.add_theme_stylebox_override("panel", panel_box(tc))
	var outer := vbox(10)
	p.add_child(outer)
	var head := hbox(10)
	var style := UiTokens.header_style()
	if style == "cap" or style == "bar":
		var cap := Panel.new()
		var cs := box(tc, 11 if style == "cap" else 0, Color.TRANSPARENT, 0, Vector4.ZERO)
		if style == "cap":
			cs.corner_radius_top_right = 3
			cs.corner_radius_bottom_right = 3
		cap.add_theme_stylebox_override("panel", cs)
		cap.custom_minimum_size = Vector2(30 if style == "cap" else 8, 22)
		cap.size_flags_vertical = Control.SIZE_SHRINK_CENTER
		head.add_child(cap)
	elif style == "bracket":
		head.add_child(label("[", "", 22, tc))
	elif style == "hairline":
		var dot := ColorRect.new()
		dot.color = tc
		dot.custom_minimum_size = Vector2(6, 6)
		dot.size_flags_vertical = Control.SIZE_SHRINK_CENTER
		head.add_child(dot)
	if icon_name != "":
		head.add_child(UiIcon.make(icon_name, 20, tc))
	var t := label(title.to_upper(), "", 22, tc)
	t.add_theme_font_override("font", font("display", 600))
	head.add_child(t)
	if style == "bracket":
		head.add_child(label("]", "", 22, tc))
	if sub != "":
		var s := label(sub.to_upper(), "Caption")
		s.size_flags_vertical = Control.SIZE_SHRINK_END
		head.add_child(s)
	var line := ColorRect.new()
	line.color = Color(tc, 0.3 if style != "hairline" else 0.5)
	line.custom_minimum_size = Vector2(0, 2 if style != "hairline" else 1)
	line.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	line.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	head.add_child(line)
	if style == "cap":
		var endcap := Panel.new()
		endcap.add_theme_stylebox_override("panel", box(Color(tc, 0.55), 5, Color.TRANSPARENT, 0, Vector4.ZERO))
		endcap.custom_minimum_size = Vector2(14, 10)
		endcap.size_flags_vertical = Control.SIZE_SHRINK_CENTER
		head.add_child(endcap)
	outer.add_child(head)
	var body := vbox(8)
	body.size_flags_vertical = Control.SIZE_EXPAND_FILL
	outer.add_child(body)
	p.set_meta("body", body)
	p.set_meta("head", head)
	return p

static func body(frame_panel: Control) -> VBoxContainer:
	return frame_panel.get_meta("body")

static func card() -> PanelContainer:
	var p := PanelContainer.new()
	p.theme_type_variation = "Card"
	return p

## A horizontal meter: the LCARS segmented bar (see LcarsMeter), still a ProgressBar so callers can set `value`.
static func meter(value: float, max_value: float, color: Color = Color(0, 0, 0, 0), height: int = 8, width: int = 0) -> ProgressBar:
	var bar := LcarsMeter.new()
	bar.max_value = max(1.0, max_value)
	bar.value = clamp(value, 0.0, bar.max_value)
	bar.show_percentage = false
	bar.custom_minimum_size = Vector2(width, height)
	bar.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	bar.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	bar.fill_color = color if color.a > 0 else accent()
	return bar

## The meter bar: an Okudagram run of blocks separated by black gaps, the left end rounded in the looks that round their pills and
## square in TOS and the movie looks. Under 6 px high it is one solid bar with quarter gaps. Replaces the engine's two rounded boxes.
class LcarsMeter extends ProgressBar:
	var fill_color := Color.WHITE
	func _draw() -> void:
		var h := size.y
		var w := size.x
		if w < 4 or h < 1:
			return
		var f: float = clamp(value / max(0.0001, max_value), 0.0, 1.0)
		var track := Color(Lcars.PANEL3, 0.95)
		var round_left := UiTokens.pill_radius() >= 10 and h >= 4
		var seg_w: float = clamp(h * 2.6, 7.0, 16.0)
		if h < 6.0:
			_run(0.0, w, h, track, round_left)
			if f > 0.0:
				_run(0.0, w * f, h, fill_color, round_left)
			for q in [0.25, 0.5, 0.75]:
				draw_rect(Rect2(w * q - 0.75, 0, 1.5, h), Lcars.BG)
			return
		var n: int = maxi(3, int(w / (seg_w + 2.0)))
		var each := (w - 2.0 * (n - 1)) / n
		for i in n:
			var x0 := i * (each + 2.0)
			var lo := float(i) / n
			var hi := float(i + 1) / n
			var rl: bool = round_left and i == 0
			_seg(x0, each, h, track, rl)
			if f > lo:
				var part: float = clamp((f - lo) / (hi - lo), 0.0, 1.0)
				_seg(x0, each * part, h, fill_color, rl)
	func _seg(x0: float, sw: float, h: float, col: Color, round_end: bool) -> void:
		if sw <= 0.0:
			return
		_run(x0, x0 + sw, h, col, round_end)
	func _run(x0: float, x1: float, h: float, col: Color, round_end: bool) -> void:
		if round_end:
			var r: float = min(h / 2.0, x1 - x0)
			draw_circle(Vector2(x0 + r, h / 2.0), r, col)
			draw_rect(Rect2(x0 + r, 0, max(0.0, x1 - x0 - r), h), col)
		else:
			draw_rect(Rect2(x0, 0, x1 - x0, h), col)

## A flat glyph button drawn from the UI icon set (delta, cross, pin ...), tinted by state.
class GlyphButton extends Button:
	var glyph := "delta_right"
	var px := 22.0   # the Trek control pill (18 Nov) needs room to read
	func _draw() -> void:
		var col := Lcars.TEXT2
		if disabled:
			col = Color(Lcars.TEXT3, 0.5)
		elif is_pressed():
			col = Lcars.accent().lightened(0.3)
		elif is_hovered():
			col = Lcars.accent()
		UiIcon.draw_icon(self, glyph, Rect2((size - Vector2(px, px)) / 2.0, Vector2(px, px)), col)

## A stat: small caption over a large value, explained on hover. `tip` is a string or an explanation Dictionary
## (docs/UI-DESIGN-SYSTEM.md §8); with none, the caption and value form the explanation's title.
static func stat(caption_text: String, value: Variant, tone: String = "", tip: Variant = "") -> Control:
	var ex := RichTip.from(tip)
	if str(ex.get("title", "")) == "":
		ex["title"] = caption_text
	elif not ex.has("value") and not ex.has("parts"):
		ex = {"title": caption_text, "value": num(value), "effect": str(ex["title"])}
	if not ex.has("value"):
		ex["value"] = num(value)
	var host := RichTip.Explained.new(ex)
	var v := vbox(0)
	v.add_child(caption(caption_text))
	var val := label(num(value), "", 24, tone_color(tone) if tone != "" else TEXT)
	val.add_theme_font_override("font", font("display", 560))
	v.add_child(val)
	ignore_mouse(v)
	host.add_child(v)
	return host

## A key-figure tile with icon, delta and rich explanation (KpiTile).
static func kpi(icon_name: String, caption_text: String, value: Variant, delta: String = "", tone: String = "", explain: Variant = "", compact: bool = false) -> KpiTile:
	return KpiTile.make(icon_name, caption_text, num(value), delta, tone, explain, compact)

## An icon from the UI set (UiIcon).
static func icon(name_: String, px: float = 20.0, tint: Variant = null) -> UiIcon:
	return UiIcon.make(name_, px, tint)

## An icon with a text beside it (the mark that used to be a text glyph such as a diamond).
static func marked(icon_name: String, text: String, variation: String = "", size: int = 0, color: Variant = null, px: float = 14.0) -> HBoxContainer:
	var h := hbox(5)
	h.add_child(UiIcon.make(icon_name, px, (color if color != null else TEXT2)))
	h.add_child(label(text, variation, size, color))
	return h

static func badge(text: String, tone: String = "primary", icon_name: String = "") -> PanelContainer:
	var p := PanelContainer.new()
	var c := tone_color(tone)
	p.add_theme_stylebox_override("panel", box_tab(Color(c, 0.18), 9, 2, Color(c, 0.7), 1, Vector4(8, 1, 8, 1)))
	var l := label(text.to_upper(), "", 12, c)
	l.add_theme_font_override("font", font("display", 600))
	if icon_name != "":
		var hb := hbox(4)
		hb.add_child(UiIcon.make(icon_name, 12, c))
		hb.add_child(l)
		p.add_child(hb)
	else:
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

## Draws a faction emblem into a CanvasItem at centre c with size s (routes to Insignia; `empire` may also be a power key
## such as "dominion" or "borg"). `bg` is the surface behind it, for the mark's cut-outs.
static func draw_emblem(ci: CanvasItem, empire: Variant, c: Vector2, s: float, color: Color, outline: bool = false, bg: Variant = null) -> void:
	Insignia.draw(ci, empire, Rect2(c - Vector2(s, s) / 2.0, Vector2(s, s)), color, "outline" if outline else "auto", bg)

static func emblem(empire: Variant, size: float = 28.0, color: Variant = null) -> Control:
	var e := EmblemView.new()
	e.empire = empire
	e.custom_minimum_size = Vector2(size, size)
	e.tint = (owner_color(int(empire)) if empire is int else TEXT2) if color == null else color
	return e

class EmblemView extends Control:
	var empire: Variant = 0
	var tint := Color.WHITE
	var bg: Variant = null
	var style := "auto"  ## "seal" draws the Federation's UFP seal (other powers keep their mark)
	func _ready() -> void:
		mouse_filter = Control.MOUSE_FILTER_IGNORE
	func _draw() -> void:
		if style == "seal":
			var s: float = min(size.x, size.y)
			Insignia.draw(self, empire, Rect2(size / 2.0 - Vector2(s, s) / 2.0, Vector2(s, s)), tint, "seal", bg)
		else:
			Lcars.draw_emblem(self, empire, size / 2.0, min(size.x, size.y), tint, false, bg)

# ---------------------------------------------------------------------------------------------
# Small raster textures for theme icons (switches, lamps, the dropdown delta): drawn in code, cached
# ---------------------------------------------------------------------------------------------
static var _tex := {}

static func _cached(key: String, w: int, h: int, painter: Callable) -> ImageTexture:
	if not _tex.has(key):
		var img := Image.create(w, h, false, Image.FORMAT_RGBA8)
		img.fill(Color(0, 0, 0, 0))
		painter.call(img)
		_tex[key] = ImageTexture.create_from_image(img)
	return _tex[key]

static func _px_rect(img: Image, x0: int, y0: int, x1: int, y1: int, col: Color) -> void:
	for y in range(maxi(0, y0), mini(img.get_height(), y1)):
		for x in range(maxi(0, x0), mini(img.get_width(), x1)):
			img.set_pixel(x, y, col)

## A bar with a round left end (cap) of radius h/2, square on the right.
static func _px_tab(img: Image, x0: int, y0: int, x1: int, y1: int, col: Color, round_left: bool = true) -> void:
	var r := (y1 - y0) / 2.0
	for y in range(y0, y1):
		for x in range(x0, x1):
			var ok := true
			if round_left and x < x0 + r:
				var dx := (x + 0.5) - (x0 + r)
				var dy := (y + 0.5) - (y0 + r)
				ok = dx * dx + dy * dy <= r * r
			if ok:
				img.set_pixel(x, y, col)

## Fills a polygon with 3x3 supersampling (even-odd), scaling `col`'s alpha by coverage.
static func _px_poly(img: Image, pts: PackedVector2Array, col: Color) -> void:
	var lo := Vector2(1e9, 1e9)
	var hi := Vector2(-1e9, -1e9)
	for p in pts:
		lo = Vector2(min(lo.x, p.x), min(lo.y, p.y))
		hi = Vector2(max(hi.x, p.x), max(hi.y, p.y))
	for y in range(maxi(0, int(floor(lo.y))), mini(img.get_height(), int(ceil(hi.y)) + 1)):
		for x in range(maxi(0, int(floor(lo.x))), mini(img.get_width(), int(ceil(hi.x)) + 1)):
			var hit := 0
			for sy in 3:
				for sx in 3:
					if Geometry2D.is_point_in_polygon(Vector2(x + (sx + 0.5) / 3.0, y + (sy + 0.5) / 3.0), pts):
						hit += 1
			if hit > 0:
				img.set_pixel(x, y, Color(col, col.a * hit / 9.0))

## The Starfleet delta as a small theme icon (the dropdown arrow), tip rotated by `ang` (PI points it down).
static func delta_texture(col: Color, px: int, ang: float = 0.0) -> ImageTexture:
	return _cached("delta:%s:%d:%.2f" % [col.to_html(), px, ang], px, px, func(img: Image):
		_px_poly(img, UiIcon.delta_points(Vector2(px, px) / 2.0, px * 0.46, ang), col))

## The two stacked deltas of a spin box.
static func updown_texture(col: Color) -> ImageTexture:
	return _cached("updown:" + col.to_html(), 10, 16, func(img: Image):
		_px_poly(img, UiIcon.delta_points(Vector2(5, 4.2), 3.6, 0.0), col)
		_px_poly(img, UiIcon.delta_points(Vector2(5, 11.8), 3.6, PI), col))

## A rocker switch in LCARS blocks: a track with a round left end and a square block that sits on the lit side.
static func switch_texture(col: Color, on: bool) -> ImageTexture:
	return _cached("switch:%s:%s" % [col.to_html(), on], 46, 22, func(img: Image):
		_px_tab(img, 0, 2, 46, 20, Color(col, 0.28) if on else Color(PANEL3, 1.0), true)
		if on:
			_px_rect(img, 25, 0, 45, 22, col)
			_px_rect(img, 8, 9, 20, 13, Color(col, 0.55))
		else:
			_px_rect(img, 3, 0, 23, 22, Color(TEXT3, 0.9))
			_px_rect(img, 28, 9, 40, 13, Color(TEXT3, 0.35)))

## A check lamp: a square housing, lit solid when on.
static func lamp_texture(col: Color, on: bool) -> ImageTexture:
	return _cached("lamp:%s:%s" % [col.to_html(), on], 20, 20, func(img: Image):
		_px_rect(img, 1, 1, 19, 19, Color(col, 0.9) if on else Color(TEXT3, 0.7))
		_px_rect(img, 3, 3, 17, 17, Color(PANEL_SOLID, 1.0))
		if on:
			_px_rect(img, 5, 5, 15, 15, col))

## A slider knob: a tall LCARS block.
static func slider_knob_texture(col: Color) -> ImageTexture:
	return _cached("knob:" + col.to_html(), 12, 22, func(img: Image):
		_px_rect(img, 0, 0, 12, 22, col)
		_px_rect(img, 5, 4, 7, 18, Color(BG, 0.55)))

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
static func row_button(active: bool, height: int = 52, radius: int = -1) -> Button:
	if radius < 0:
		radius = maxi(4, UiTokens.radius() - 2)
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

## The text marks that used to stand for icons, and the Starfleet glyph each one is now.
const TEXT_GLYPHS := {"▾": "delta_down", "▸": "delta_right", "▴": "delta_up", "◂": "delta_left", "▲": "delta_up", "▼": "delta_down",
	"✕": "cross", "◆": "pin_on", "◇": "pin", "✓": "tick", "✗": "cross"}

## A flat glyph button with a disabled reason. A text mark from TEXT_GLYPHS is drawn as that glyph.
static func icon_button(text: String, hint: String, reason: String, on: Callable) -> Button:
	if TEXT_GLYPHS.has(text):
		var g := GlyphButton.new()
		g.glyph = TEXT_GLYPHS[text]
		g.flat = true
		g.custom_minimum_size = Vector2(30, 30)
		g.focus_mode = Control.FOCUS_ALL
		g.tooltip_text = ("Unavailable: " + reason) if reason != "" else hint
		g.disabled = reason != ""
		g.pressed.connect(on)
		return g
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
