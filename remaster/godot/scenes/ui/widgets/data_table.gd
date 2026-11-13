class_name DataTable
extends VBoxContainer
## Sortable table. Columns: {key, label, value: Callable(row) -> sort value, text: Callable(row) -> String
## (optional), color: Callable(row) -> Color (optional), num: bool, width: int}. Click a header to sort.

signal row_activated(row: Dictionary)

var columns: Array = []
var rows: Array = []
var sort_key := ""
var sort_dir := 1
var max_rows := 400

static func make(cols: Array, data: Array, initial: String = "") -> DataTable:
	var t := DataTable.new()
	t.columns = cols
	t.rows = data
	t.sort_key = initial if initial != "" else str(cols[0]["key"])
	t.add_theme_constant_override("separation", 0)
	t.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	t._rebuild()
	return t

func set_rows(data: Array) -> void:
	rows = data
	_rebuild()

func _col() -> Dictionary:
	for c in columns:
		if c["key"] == sort_key:
			return c
	return columns[0]

func _rebuild() -> void:
	for c in get_children():
		c.queue_free()
	var head := HBoxContainer.new()
	head.add_theme_constant_override("separation", 0)
	for c in columns:
		var b := Button.new()
		b.flat = true
		b.text = str(c["label"]).to_upper() + ("     " if c["key"] == sort_key else "")
		if c["key"] == sort_key:
			# the sort marker is the Starfleet delta, pointing the way the list runs
			var sd := UiIcon.make("delta_up" if sort_dir > 0 else "delta_down", 10, Lcars.accent())
			sd.set_anchors_preset(Control.PRESET_CENTER_RIGHT)
			sd.offset_left = -16
			sd.offset_right = -6
			sd.offset_top = -5
			sd.offset_bottom = 5
			b.add_child(sd)
		b.alignment = HORIZONTAL_ALIGNMENT_RIGHT if c.get("num", false) else HORIZONTAL_ALIGNMENT_LEFT
		b.add_theme_font_override("font", Lcars.font("display", 560))
		b.add_theme_font_size_override("font_size", 14)
		b.add_theme_color_override("font_color", Lcars.accent() if c["key"] == sort_key else Lcars.TEXT3)
		b.add_theme_color_override("font_hover_color", Lcars.TEXT)
		_size(b, c)
		b.tooltip_text = c.get("tip", "")
		var key: String = c["key"]
		var numeric: bool = c.get("num", false)
		b.pressed.connect(func():
			if sort_key == key:
				sort_dir = -sort_dir
			else:
				sort_key = key
				sort_dir = -1 if numeric else 1
			_rebuild())
		head.add_child(b)
	add_child(head)
	var line := ColorRect.new()
	line.color = Lcars.LINE2
	line.custom_minimum_size.y = 1
	add_child(line)
	var col := _col()
	var getter: Callable = col["value"]
	var sorted := rows.duplicate()
	sorted.sort_custom(func(a, b):
		var x: Variant = getter.call(a)
		var y: Variant = getter.call(b)
		if typeof(x) in [TYPE_INT, TYPE_FLOAT] and typeof(y) in [TYPE_INT, TYPE_FLOAT]:
			return (x < y) if sort_dir > 0 else (x > y)
		return (str(x).naturalnocasecmp_to(str(y)) < 0) if sort_dir > 0 else (str(x).naturalnocasecmp_to(str(y)) > 0))
	var n := 0
	for r in sorted:
		if n >= max_rows:
			break
		n += 1
		var row := Button.new()
		row.flat = false
		row.custom_minimum_size.y = 34
		var sb := Lcars.box(Color(Lcars.PANEL2, 0.35) if n % 2 == 0 else Color(0, 0, 0, 0), 6, Color.TRANSPARENT, 0, Vector4.ZERO)
		row.add_theme_stylebox_override("normal", sb)
		row.add_theme_stylebox_override("hover", Lcars.box(Color(Lcars.accent(), 0.16), 6, Color.TRANSPARENT, 0, Vector4.ZERO))
		row.add_theme_stylebox_override("pressed", Lcars.box(Color(Lcars.accent(), 0.26), 6, Color.TRANSPARENT, 0, Vector4.ZERO))
		row.add_theme_stylebox_override("focus", Lcars.box(Color(0, 0, 0, 0), 6, Lcars.accent(), 1, Vector4.ZERO))
		var cells := HBoxContainer.new()
		cells.add_theme_constant_override("separation", 0)
		cells.set_anchors_preset(Control.PRESET_FULL_RECT)
		cells.mouse_filter = Control.MOUSE_FILTER_IGNORE
		for c in columns:
			var txt: String = c["text"].call(r) if c.has("text") else Lcars.num(c["value"].call(r))
			var l := Lcars.label(txt, "Mono" if c.get("num", false) else "", 14)
			l.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT if c.get("num", false) else HORIZONTAL_ALIGNMENT_LEFT
			l.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
			l.clip_text = true
			l.text_overrun_behavior = TextServer.OVERRUN_TRIM_ELLIPSIS
			if c.has("color"):
				l.add_theme_color_override("font_color", c["color"].call(r))
			_size(l, c)
			l.mouse_filter = Control.MOUSE_FILTER_IGNORE
			l.size_flags_vertical = Control.SIZE_EXPAND_FILL
			var pad := MarginContainer.new()
			pad.add_theme_constant_override("margin_left", 10)
			pad.add_theme_constant_override("margin_right", 10)
			pad.mouse_filter = Control.MOUSE_FILTER_IGNORE
			_size(pad, c)
			l.size_flags_horizontal = Control.SIZE_EXPAND_FILL
			pad.add_child(l)
			cells.add_child(pad)
		row.add_child(cells)
		var data: Dictionary = r
		row.pressed.connect(func(): row_activated.emit(data))
		add_child(row)

func _size(ctrl: Control, c: Dictionary) -> void:
	var w := int(c.get("width", 0))
	if w > 0:
		ctrl.custom_minimum_size.x = w
	else:
		ctrl.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		ctrl.size_flags_stretch_ratio = float(c.get("ratio", 1.0))
