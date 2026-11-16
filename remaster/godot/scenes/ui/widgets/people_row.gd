class_name PeopleRow
extends Control
## Workforce as people: one figure per `unit` workers, filled for the workers assigned, an outline for each free job.
## Click a figure to staff up to it, click the last filled one to take one unit off, drag to sweep. `changed(value)` fires as the
## row moves (the screen previews with the core), `released` when the mouse comes up (the screen applies).

signal changed(value: int)
signal released

var value := 0     ## workers assigned
var limit := 0     ## jobs available
var unit := 1      ## workers per figure
var col := Color.WHITE
var editable := true
var _hover := -1
var _drag := false

const MAX_SLOTS := 16
const NICE := [1, 2, 5, 10, 20, 25, 50, 100, 200]

static func unit_for(jobs: int) -> int:
	for u in NICE:
		if ceili(float(maxi(jobs, 1)) / u) <= MAX_SLOTS:
			return u
	return 500

static func make(value_: int, limit_: int, color_: Color, editable_: bool, tip: String) -> PeopleRow:
	var p := PeopleRow.new()
	p.limit = limit_
	p.unit = unit_for(limit_)
	p.value = value_
	p.col = color_
	p.editable = editable_ and limit_ > 0
	p.custom_minimum_size = Vector2(160, 30)
	p.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	p.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	p.tooltip_text = tip + ("\nOne figure = %d workers. Click or drag to staff." % p.unit if p.unit > 1 else "\nClick or drag to staff.")
	p.mouse_filter = Control.MOUSE_FILTER_STOP
	if p.editable:
		p.mouse_default_cursor_shape = Control.CURSOR_POINTING_HAND
	return p

func slots() -> int:
	return maxi(1, ceili(float(limit) / unit))

func _slot_px() -> float:
	var n := slots()
	return clampf((size.x - (n - 1) * 3.0) / n, 10.0, 26.0)

func _filled() -> int:
	return ceili(float(value) / unit) if value > 0 else 0

func _slot_at(x: float) -> int:
	return clampi(int(floor(x / (_slot_px() + 3.0))), 0, slots() - 1)

func set_value_no_signal(v: int) -> void:
	value = v
	queue_redraw()

func _draw() -> void:
	var s := _slot_px()
	var f := _filled()
	var top := (size.y - s) / 2.0
	for i in slots():
		var r := Rect2(Vector2(i * (s + 3.0), top), Vector2(s, s))
		var on := i < f
		var c := col if on else Color(col, 0.2)
		if editable and _hover >= 0 and not _drag and i <= _hover and not on:
			c = Color(col, 0.45)
		UiIcon.draw_icon(self, "population", r, c)
	if limit <= 0:
		draw_string(Lcars.font("ui", 500), Vector2(2, size.y / 2.0 + 5), "-", HORIZONTAL_ALIGNMENT_LEFT, -1, 14, Lcars.TEXT3)

func _gui_input(event: InputEvent) -> void:
	if not editable:
		return
	var mb := event as InputEventMouseButton
	if mb != null and mb.button_index == MOUSE_BUTTON_LEFT:
		if mb.pressed:
			var i := _slot_at(mb.position.x)
			_drag = true
			# the last filled figure again takes one unit off
			_move_to(i * unit if i + 1 == _filled() else mini((i + 1) * unit, limit))
		elif _drag:
			_drag = false
			released.emit()
		accept_event()
		return
	var mm := event as InputEventMouseMotion
	if mm != null:
		var i := _slot_at(mm.position.x)
		if _drag:
			_move_to(mini((i + 1) * unit, limit) if mm.position.x >= 0.0 else 0)
		elif i != _hover:
			_hover = i
			queue_redraw()

func _notification(what: int) -> void:
	if what == NOTIFICATION_MOUSE_EXIT:
		_hover = -1
		queue_redraw()

func _move_to(v: int) -> void:
	if v == value:
		return
	value = v
	queue_redraw()
	changed.emit(v)
