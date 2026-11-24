class_name KpiTile
extends PanelContainer
## A key-figure tile (docs/UI-DESIGN-SYSTEM.md §5): icon, caption, the figure, a signed delta, and the rich
## explanation on hover. `compact` drops the panel for the top bar.

var ex := {}

static func make(icon: String, caption: String, value: String, delta: String = "", tone: String = "", explain: Variant = "", compact: bool = false) -> KpiTile:
	var t := KpiTile.new()
	t.ex = RichTip.from(explain)
	if not t.ex.has("title") or str(t.ex["title"]) == "":
		t.ex["title"] = caption
	if not t.ex.has("value"):
		t.ex["value"] = value + ("  " + delta if delta != "" else "")
	t.tooltip_text = RichTip.text(t.ex)
	t.mouse_filter = Control.MOUSE_FILTER_STOP
	if compact:
		t.add_theme_stylebox_override("panel", Lcars.box_tab(Color.TRANSPARENT, 8, 2, Color.TRANSPARENT, 0, Vector4(4, 2, 6, 2)))
	else:
		# an LCARS tab: a heavy coloured cap on the left with the rounded end, the right end square
		var sb := Lcars.box_tab(Color(Lcars.PANEL2, 0.75), UiTokens.radius(), 2, Lcars.LINE, 1, Vector4(16, 10, 16, 10))
		sb.border_width_left = 6
		sb.border_color = Color(Lcars.tone_color(tone) if tone != "" else Lcars.accent(), 0.85)
		t.add_theme_stylebox_override("panel", sb)
		t.custom_minimum_size.x = 150
	var row := Lcars.hbox(10)
	t.add_child(row)
	var tint := Lcars.tone_color(tone) if tone != "" else Lcars.accent()
	row.add_child(UiIcon.make(icon, 22 if compact else 28, tint))   # Trek icon set: solid marks, sized to read at bar size
	var col := Lcars.vbox(-3 if compact else 0)
	col.add_child(Lcars.caption(caption))
	var vr := Lcars.hbox(6)
	var v := Lcars.label(value, "", 22 if compact else 30, Lcars.TEXT)
	v.name = "Value"
	v.add_theme_font_override("font", Lcars.font("display", 560))
	vr.add_child(v)
	if delta != "":
		var dc := Lcars.TEXT3
		if delta.begins_with("+") and delta != "+0":
			dc = Lcars.GOOD
		elif delta.begins_with("-") and delta != "-0":
			dc = Lcars.ALERT
		var d := Lcars.label(delta, "", 15 if compact else 17, dc)
		d.add_theme_font_override("font", Lcars.font("display", 500))
		d.size_flags_vertical = Control.SIZE_SHRINK_END
		vr.add_child(d)
	col.add_child(vr)
	row.add_child(col)
	Lcars.ignore_mouse(row)
	return t

## The tile's tooltip as plain text: the one string a hover and a long-press both show.
func tip_text() -> String:
	return RichTip.text(ex)

func _make_custom_tooltip(_for_text: String) -> Object:
	return RichTip.build(ex)
