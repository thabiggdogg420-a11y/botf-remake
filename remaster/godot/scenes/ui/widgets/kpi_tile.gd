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
		t.add_theme_stylebox_override("panel", Lcars.box(Color.TRANSPARENT, 8, Color.TRANSPARENT, 0, Vector4(4, 2, 6, 2)))
	else:
		var sb := Lcars.box(Color(Lcars.PANEL2, 0.75), UiTokens.radius(), Lcars.LINE, 1, Vector4(14, 10, 16, 10))
		sb.border_width_left = 3
		sb.border_color = Color(Lcars.tone_color(tone) if tone != "" else Lcars.accent(), 0.7)
		t.add_theme_stylebox_override("panel", sb)
		t.custom_minimum_size.x = 150
	var row := Lcars.hbox(10)
	t.add_child(row)
	var tint := Lcars.tone_color(tone) if tone != "" else Lcars.accent()
	row.add_child(UiIcon.make(icon, 18 if compact else 24, tint))
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

func _make_custom_tooltip(_for_text: String) -> Object:
	return RichTip.build(ex)
