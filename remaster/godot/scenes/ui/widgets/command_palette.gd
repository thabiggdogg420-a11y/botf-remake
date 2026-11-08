class_name CommandPalette
extends VBoxContainer
## Ctrl+K command palette (docs/UI-DESIGN-SYSTEM.md §9): a search field over a filtered list of items
## [{id, label, sub, icon, group}]. Up and Down move, Enter runs, Esc closes (the shell's modal handling).
## `on_run(id)` is called with the chosen item's id; the shell decides what it does.

signal ran(id: String)

var items: Array = []
var field: LineEdit
var list: VBoxContainer
var _shown: Array = []
var _sel := 0

static func make(items_: Array) -> CommandPalette:
	var p := CommandPalette.new()
	p.name = "CommandPalette"
	p.items = items_
	p.custom_minimum_size = Vector2(640, 0)
	p.add_theme_constant_override("separation", 10)
	var head := Lcars.hbox(10)
	head.add_child(UiIcon.make("search", 22, Lcars.accent()))
	p.field = LineEdit.new()
	p.field.name = "Query"
	p.field.placeholder_text = "Go to a screen, colony, fleet or command..."
	p.field.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	p.field.add_theme_font_override("font", Lcars.font("ui", 450))
	p.field.add_theme_font_size_override("font_size", 18)
	p.field.custom_minimum_size.y = 44
	head.add_child(p.field)
	p.add_child(head)
	p.list = Lcars.vbox(2)
	p.list.name = "Results"
	var sc := Lcars.scroll(p.list)
	sc.custom_minimum_size = Vector2(0, 420)
	p.add_child(sc)
	var hint := Lcars.label("UP / DOWN TO MOVE  ·  ENTER TO RUN  ·  ESC TO CLOSE", "Caption")
	p.add_child(hint)
	p.field.text_changed.connect(func(_t): p._sel = 0; p._fill())
	p.field.gui_input.connect(p._on_key)
	p.field.text_submitted.connect(func(_t): p.run_selected())
	p._fill()
	return p

## Items whose label or sub contains every word of `q` (case-insensitive), best (label prefix) first.
static func filter(all: Array, q: String) -> Array:
	var words := q.to_lower().strip_edges().split(" ", false)
	if words.is_empty():
		return all.slice(0, 40)
	var hits := []
	for it in all:
		var hay := (str(it.get("label", "")) + " " + str(it.get("sub", "")) + " " + str(it.get("group", ""))).to_lower()
		var ok := true
		for w in words:
			if hay.find(w) < 0:
				ok = false
				break
		if ok:
			hits.append(it)
	hits.sort_custom(func(a, b): return str(a["label"]).to_lower().begins_with(words[0]) and not str(b["label"]).to_lower().begins_with(words[0]))
	return hits.slice(0, 40)

func _fill() -> void:
	for c in list.get_children():
		c.queue_free()
	_shown = filter(items, field.text)
	if _shown.is_empty():
		list.add_child(Lcars.empty_note("Nothing matches."))
		return
	for n in _shown.size():
		var it: Dictionary = _shown[n]
		var b := Lcars.row_button(n == _sel, 44, 10)
		b.name = "Item_" + str(it["id"]).validate_node_name()
		var row := Lcars.row_inner(b, 12, 12)
		row.add_child(UiIcon.make(str(it.get("icon", "arrow_right")), 18, Lcars.accent()))
		var l := Lcars.label(str(it["label"]), "", 16, Lcars.TEXT)
		l.size_flags_vertical = Control.SIZE_SHRINK_CENTER
		row.add_child(l)
		row.add_child(Lcars.spacer())
		var s := Lcars.label(str(it.get("sub", "")).to_upper(), "Caption")
		s.size_flags_vertical = Control.SIZE_SHRINK_CENTER
		row.add_child(s)
		Lcars.ignore_mouse(row)
		var id := str(it["id"])
		b.pressed.connect(func(): ran.emit(id))
		list.add_child(b)

func _on_key(ev: InputEvent) -> void:
	var k := ev as InputEventKey
	if k == null or not k.pressed:
		return
	if k.keycode == KEY_DOWN or k.keycode == KEY_UP:
		if _shown.is_empty():
			return
		_sel = (_sel + (1 if k.keycode == KEY_DOWN else -1) + _shown.size()) % _shown.size()
		_fill()
		field.accept_event()

func run_selected() -> void:
	if _sel < _shown.size():
		ran.emit(str(_shown[_sel]["id"]))
