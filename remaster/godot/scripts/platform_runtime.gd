class_name PlatformRuntime
extends Node
## The running half of mobile mode (see PlatformMode): finger gestures, long-press tooltips and the slow-frame render scale.
## App adds one of these when the mode is mobile; on desktop it does not exist, so desktop input and timing are untouched.
##   * long-press: a finger held still 0.5 s on a control that has a tooltip opens that tooltip (the same rich one a hover
##     opens) beside the finger; the lift that follows is swallowed so it does not also click the control. A control (or its
##     parent) may define on_long_press(local_pos) -> bool to take the press itself (the star chart shows its hover card).
##   * pinch: two fingers zoom whatever under them defines on_pinch(factor, local_center) (the star chart).
##   * render scale: the 3D views drop to RENDER_MIN in steps while frames stay slow, never rising again in a session.

const TOOLTIP_SEC := 4.0
const RENDER_MIN := 0.6
const RENDER_STEP := 0.1
const SLOW_MS := 42.0      ## an average frame over this (under 24 fps) is slow
const WINDOW_SEC := 1.5    ## frames are averaged over this long
const SETTLE_SEC := 3.0    ## after a step, wait this long before judging again
const START_SEC := 10.0    ## nothing is judged while the game is still starting (shader compiles are long frames, not slow rendering)
const HITCH_CLAMP := 0.1   ## one long frame counts as at most this: a hitch must not read as a slow average

var gestures := TouchGestures.new()
var app: Node = null
var _tip_layer: CanvasLayer = null
var _tip_age := 0.0
var _swallow := false      ## the next mouse release (the lift after a long-press that opened a tooltip) must not click
var _acc_t := 0.0
var _acc_n := 0
var _settle := START_SEC
var steps_taken := 0

func _ready() -> void:
	name = "PlatformRuntime"
	process_mode = Node.PROCESS_MODE_ALWAYS
	gestures.long_press.connect(_on_long_press)
	gestures.pinch.connect(_on_pinch)

func _input(event: InputEvent) -> void:
	var st := event as InputEventScreenTouch
	if st != null or event is InputEventScreenDrag:
		if st != null and st.pressed:
			_swallow = false
			hide_tooltip()
		gestures.feed(event)
		_scroll_by_finger(event)
	var mb := event as InputEventMouseButton
	if _swallow and mb != null and not mb.pressed and mb.button_index == MOUSE_BUTTON_LEFT:
		# The lift that ends a long-press must not click what was pressed: it is delivered as a release far off the control.
		_swallow = false
		var away := mb.duplicate() as InputEventMouseButton
		away.position = Vector2(-9999, -9999)
		away.global_position = away.position
		get_viewport().set_input_as_handled()
		get_viewport().push_input(away, true)

func _process(delta: float) -> void:
	gestures.tick(delta)
	if _tip_layer != null:
		_tip_age += delta
		if _tip_age >= TOOLTIP_SEC:
			hide_tooltip()
	_watch_frames(delta)


# ---- slow-frame render scale -------------------------------------------------------------------------

## The next render cap for an average frame of `avg_ms` at cap `cur`: one step down while slow, floor RENDER_MIN.
static func next_scale(avg_ms: float, cur: float) -> float:
	if avg_ms > SLOW_MS and cur > RENDER_MIN + 0.001:
		return maxf(RENDER_MIN, snappedf(cur - RENDER_STEP, 0.01))
	return cur

func _watch_frames(delta: float) -> void:
	if _settle > 0.0:
		_settle -= delta
		return
	_acc_t += minf(delta, HITCH_CLAMP)
	_acc_n += 1
	if _acc_t < WINDOW_SEC:
		return
	var avg_ms := 1000.0 * _acc_t / maxf(_acc_n, 1)
	_acc_t = 0.0
	_acc_n = 0
	var nxt := next_scale(avg_ms, PlatformMode.render_cap)
	if nxt != PlatformMode.render_cap:
		PlatformMode.render_cap = nxt
		steps_taken += 1
		_settle = SETTLE_SEC
		reapply_render()

func reapply_render() -> void:
	var s: Dictionary = app.settings if app != null else {}
	DisplaySettings.fill_defaults(s)
	var tree := get_tree()
	DisplaySettings.apply_viewport(tree.root, s)
	for v in tree.root.find_children("*", "SubViewport", true, false):
		DisplaySettings.apply_viewport(v, s)


# ---- gestures -------------------------------------------------------------------------------------------

func _on_long_press(pos: Vector2) -> void:
	var target := control_at(get_tree().root, pos)
	if target == null:
		return
	var c := target
	while c != null:
		var local := c.get_global_transform_with_canvas().affine_inverse() * pos
		if c.has_method("on_long_press") and c.call("on_long_press", local):
			_cancel_press()
			return
		c = c.get_parent() as Control
	var owner_ctrl := tooltip_owner(target, pos)
	if owner_ctrl != null:
		_cancel_press()
		show_tooltip(owner_ctrl, pos)

## Drag-to-scroll. A ScrollContainer only hears a touch drag when the finger lands on empty space (panels and buttons take the
## touch first), so a one-finger drag that starts over a scrollable area scrolls it here, whatever it started on. It does not
## when the drag starts on a slider, scroll bar, text field or the star chart, which own their drags.
const SCROLL_SLOP := 14.0
var _sc_start := Vector2.ZERO
var _sc_state := 0          ## 0 finger up, 1 undecided, 2 scrolling, 3 not a scroll
var _sc_target: ScrollContainer = null

func _scroll_by_finger(event: InputEvent) -> void:
	var st := event as InputEventScreenTouch
	if st != null:
		if st.index != 0:
			_sc_state = 3 if st.pressed else _sc_state
			return
		if st.pressed:
			_sc_start = st.position
			_sc_state = 1
			_sc_target = null
		else:
			_sc_state = 0
			_sc_target = null
		return
	var d := event as InputEventScreenDrag
	if d == null or d.index != 0 or gestures.finger_count() > 1:
		return
	if _sc_state == 1 and d.position.distance_to(_sc_start) > SCROLL_SLOP:
		var delta := d.position - _sc_start
		_sc_target = scroller_for(control_at(get_tree().root, _sc_start), absf(delta.x) > absf(delta.y))
		_sc_state = 2 if _sc_target != null else 3
		if _sc_state == 2:
			_cancel_press()
			# the slop already travelled is not lost
			_apply_scroll(_sc_target, delta)
			get_viewport().set_input_as_handled()
			return
	if _sc_state == 2 and is_instance_valid(_sc_target):
		_apply_scroll(_sc_target, d.relative)
		get_viewport().set_input_as_handled()

func _apply_scroll(sc: ScrollContainer, rel: Vector2) -> void:
	if can_scroll(sc, true):
		sc.scroll_horizontal = int(clampf(sc.scroll_horizontal - rel.x, 0.0, sc.get_h_scroll_bar().max_value))
	if can_scroll(sc, false):
		sc.scroll_vertical = int(clampf(sc.scroll_vertical - rel.y, 0.0, sc.get_v_scroll_bar().max_value))

## Whether `sc` has something to scroll along an axis (its mode allows it and its content is larger than it).
static func can_scroll(sc: ScrollContainer, horizontal: bool) -> bool:
	var mode := sc.horizontal_scroll_mode if horizontal else sc.vertical_scroll_mode
	if mode == ScrollContainer.SCROLL_MODE_DISABLED:
		return false
	var bar: ScrollBar = sc.get_h_scroll_bar() if horizontal else sc.get_v_scroll_bar()
	return bar.max_value > bar.page + 1.0

## The ScrollContainer a drag that started on `c` should scroll: the nearest ancestor that can scroll the way the finger moves,
## unless something on the way owns drags itself.
static func scroller_for(c: Control, mostly_horizontal: bool) -> ScrollContainer:
	var cur := c
	while cur != null:
		if cur is Range or cur is LineEdit or cur is TextEdit or cur is SubViewportContainer or cur.has_method("on_pinch"):
			return null
		var sc := cur as ScrollContainer
		if sc != null and can_scroll(sc, mostly_horizontal):
			return sc
		cur = cur.get_parent() as Control
	return null

## The finger is still down on what it pressed: slide the (emulated) mouse off it so a button forgets its press, and have the
## lift delivered as a release far from every control (see _input), so the lift that ends a long-press clicks nothing.
func _cancel_press() -> void:
	_swallow = true
	var away := InputEventMouseMotion.new()
	away.position = Vector2(-9999, -9999)
	away.global_position = away.position
	away.button_mask = MOUSE_BUTTON_MASK_LEFT
	get_viewport().push_input(away, true)

func _on_pinch(factor: float, center: Vector2) -> void:
	var target := control_at(get_tree().root, center)
	var c := target
	while c != null:
		if c.has_method("on_pinch"):
			c.call("on_pinch", factor, c.get_global_transform_with_canvas().affine_inverse() * center)
			return
		c = c.get_parent() as Control

## The topmost visible control under `pos` (viewport coordinates) that takes mouse input, found the way the engine finds the
## control under the cursor. Controls set to ignore the mouse are looked through.
static func control_at(node: Node, pos: Vector2) -> Control:
	var kids := node.get_children()
	for i in range(kids.size() - 1, -1, -1):
		var k: Node = kids[i]
		if k is CanvasItem and not (k as CanvasItem).visible:
			continue
		if k is Window or k is Viewport:
			continue   # another viewport's controls live in another coordinate space
		var hit := control_at(k, pos)
		if hit != null:
			return hit
	var c := node as Control
	if c != null and c.visible and c.mouse_filter != Control.MOUSE_FILTER_IGNORE:
		var local := c.get_global_transform_with_canvas().affine_inverse() * pos
		if Rect2(Vector2.ZERO, c.size).has_point(local):
			return c
	return null

## The control at or above `c` that has a tooltip for `pos`.
static func tooltip_owner(c: Control, pos: Vector2) -> Control:
	var cur := c
	while cur != null:
		var local := cur.get_global_transform_with_canvas().affine_inverse() * pos
		if cur.get_tooltip(local) != "":
			return cur
		cur = cur.get_parent() as Control
	return null

func show_tooltip(c: Control, pos: Vector2) -> void:
	hide_tooltip()
	var local := c.get_global_transform_with_canvas().affine_inverse() * pos
	var text := c.get_tooltip(local)
	if text == "":
		return
	var body: Control = null
	if c.get_script() != null and c.has_method("_make_custom_tooltip"):
		body = c.call("_make_custom_tooltip", text) as Control
	if body == null:
		var l := Label.new()
		l.text = text
		l.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
		l.custom_minimum_size.x = 260
		l.add_theme_font_size_override("font_size", 15)
		body = l
	var panel := PanelContainer.new()
	panel.name = "TouchTooltip"
	panel.mouse_filter = Control.MOUSE_FILTER_IGNORE
	var sb := StyleBoxFlat.new()
	sb.bg_color = Color(0.04, 0.06, 0.12, 0.97)
	sb.border_color = Lcars.accent()
	sb.set_border_width_all(2)
	sb.set_corner_radius_all(6)
	sb.set_content_margin_all(8)
	panel.add_theme_stylebox_override("panel", sb)
	panel.add_child(body)
	_tip_layer = CanvasLayer.new()
	_tip_layer.layer = 120
	_tip_layer.add_child(panel)
	add_child(_tip_layer)
	_tip_age = 0.0
	# Beside the finger, inside the screen: above it when there is room, below it otherwise.
	var vp := get_viewport().get_visible_rect().size
	await get_tree().process_frame
	if _tip_layer == null or not is_instance_valid(panel):
		return
	var sz := panel.get_combined_minimum_size()
	panel.size = sz
	var x := clampf(pos.x - sz.x * 0.5, 4.0, maxf(4.0, vp.x - sz.x - 4.0))
	var y := pos.y - sz.y - 28.0
	if y < 4.0:
		y = minf(pos.y + 36.0, maxf(4.0, vp.y - sz.y - 4.0))
	panel.position = Vector2(x, y)

func hide_tooltip() -> void:
	if _tip_layer != null:
		_tip_layer.queue_free()
		_tip_layer = null

func tooltip_text() -> String:
	if _tip_layer == null or _tip_layer.get_child_count() == 0:
		return ""
	var panel := _tip_layer.get_child(0)
	for n in panel.find_children("*", "Label", true, false):
		return (n as Label).text
	return "(custom)"

func tooltip_open() -> bool:
	return _tip_layer != null
