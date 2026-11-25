class_name AdvisorPanel
extends PanelContainer
## The advisor's LCARS card (F3): the tutorial step or the current hint, one action button, and "Not now" (or
## "Skip tutorial"). Docked bottom-left of the stage, above the map layer bar, unless that would cover the control the
## guidance points at: then the next clear corner. That control gets a pulsing ring. Hidden outside a campaign.
## Logic: Advisor (scenes/ui/advisor.gd).

var app: App
var advisor: Advisor
var current: Dictionary = {}
var folded := false
var _sig := ""
var _poll := 0.0
var _busy := false
var _hl: Control     ## AdvisorHighlight, the ring on the control the guidance points at
var _spot: Spotlight ## TutorialSpotlight: a tutorial beat dims everything but its control and pulses it

static func make(app_: App) -> AdvisorPanel:
	var p := AdvisorPanel.new()
	p.app = app_
	p.advisor = Advisor.new(app_)
	p.name = "AdvisorPanel"
	return p

func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_STOP
	set_anchors_preset(Control.PRESET_BOTTOM_LEFT)
	grow_vertical = Control.GROW_DIRECTION_BEGIN
	custom_minimum_size = Vector2(400, 0)
	app.view_changed.connect(_mark)
	app.screen_changed.connect(func(_id): _mark())
	update()

func _mark() -> void:
	_sig = ""

func _process(delta: float) -> void:
	_poll += delta
	if _poll < 0.4 or _busy:
		return
	_poll = 0.0
	var s := "%s|%s|%s|%d|%d" % [app.screen, app.has_modal(), app.view.get("meta", {}).get("turn", 0), int(app.settings.get("tutorial", 0)), app.view.hash() if not app.view.is_empty() else 0]
	if s != _sig:
		_sig = s
		update()
	elif visible:
		_place()   # the screen may have laid out or scrolled since: keep the card off its target and the ring on it

## The on-screen rects of the controls the current guidance points at (Advisor.targets), in this panel's parent space. A tutorial
## beat lists alternatives, so only the first one found counts.
func target_rects() -> Array[Rect2]:
	var out: Array[Rect2] = []
	if current.is_empty() or app == null:
		return out
	var host: Node = app.current_screen() if app.current_screen() else app
	for n in advisor.targets(current):
		if n is Rect2:
			if (n as Rect2).size.x > 0 and app.get_global_rect().intersects(n):
				out.append(n)
		else:
			var c := _find_target(host, str(n))
			if c:
				if current.get("beat", false) and _reveal(c):
					_spotlight_target.call_deferred()   # it scrolled into view: point the spotlight again once the layout has moved
				out.append(c.get_global_rect())
		if current.get("beat", false) and not out.is_empty():
			break
	return out

## A beat's target may sit in a scrolled panel (the phone layout's top bar and rail scroll): bring it into view. True when anything moved.
func _reveal(c: Control) -> bool:
	var moved := false
	var p := c.get_parent()
	while p != null:
		if p is ScrollContainer:
			var sc := p as ScrollContainer
			var before := Vector2(sc.scroll_horizontal, sc.scroll_vertical)
			sc.ensure_control_visible(c)
			moved = moved or before != Vector2(sc.scroll_horizontal, sc.scroll_vertical)
		p = p.get_parent()
	return moved

## A named control on the current screen, else anywhere in the app (a dialog). A trailing * names the first enabled button
## that matches (the first thing the player may pick).
func _find_target(host: Node, n: String) -> Control:
	for root_ in [host, app]:
		if n.ends_with("*"):
			for b in root_.find_children(n, "Button", true, false):
				if not (b as Button).disabled and (b as Button).is_visible_in_tree() and (b as Button).size.x > 0:
					return b as Button
		else:
			var c := root_.find_child(n, true, false) as Control
			if c and c.is_visible_in_tree() and c.size.x > 0:
				return c
	return null

## The slots the card may take, in order of preference: bottom-left (above the map layer bar, clear of the centre-bottom
## toasts), bottom-right, top-right and top-left under the top bar. It takes the first slot that covers none of the
## controls the guidance points at.
func slots(vp: Vector2, s: Vector2) -> Array[Vector2]:
	return [Vector2(180, vp.y - s.y - 100), Vector2(vp.x - s.x - 24, vp.y - s.y - 24), Vector2(vp.x - s.x - 24, 100), Vector2(180, 100)]

func _place() -> void:
	var vp := get_parent_area_size() if get_parent() else Vector2(1920, 1080)
	reset_size()
	var rects := target_rects()
	var pick := slots(vp, size)[0]
	for at in slots(vp, size):
		var mine := Rect2(at, size).grow(10)
		var clear := true
		for r in rects:
			clear = clear and not mine.intersects(r)
		if clear:
			pick = at
			break
	position = pick
	_highlight_target()

## Highlights the control the guidance points at: a pulsing ring in the card's colour (the first target found).
func _highlight_target() -> void:
	_spotlight_target()
	if _hl == null and get_parent():
		_hl = Highlight.new()
		_hl.name = "AdvisorHighlight"
		_hl.mouse_filter = Control.MOUSE_FILTER_IGNORE
		_hl.set_anchors_preset(Control.PRESET_FULL_RECT)
		get_parent().add_child(_hl)
		get_parent().move_child(_hl, get_index())
	if _hl:
		var rects: Array[Rect2] = []
		if visible and not folded and not current.get("beat", false):
			rects = target_rects()   # a tutorial beat is spotlit instead
		_hl.rect = rects[0] if not rects.is_empty() else Rect2()
		_hl.color = Lcars.tone_color(str(current.get("tone", "primary")))
		_hl.visible = _hl.rect.size.x > 0
		_hl.queue_redraw()

## A tutorial beat dims the whole screen but the one control it wants, and pulses that control.
func _spotlight_target() -> void:
	if _spot == null and get_parent():
		_spot = Spotlight.new()
		_spot.name = "TutorialSpotlight"
		get_parent().add_child(_spot)
		get_parent().move_child(_spot, get_index())
	if _spot:
		var rects: Array[Rect2] = []
		if visible and current.get("beat", false):
			rects = target_rects()
		_spot.point_at(rects[0] if not rects.is_empty() else Rect2())

class Highlight extends Control:
	var rect := Rect2()
	var color := Color.WHITE
	var _t := 0.0
	func _process(delta: float) -> void:
		_t += delta
		if visible:
			queue_redraw()
	func _draw() -> void:
		if rect.size.x <= 0:
			return
		var r := Rect2(rect.position - get_global_rect().position, rect.size).grow(6)
		var a := 0.55 + 0.35 * sin(_t * 4.0) if not UiTokens.reduced_motion else 0.8
		var sb := StyleBoxFlat.new()
		sb.draw_center = false
		sb.border_color = Color(color, a)
		sb.set_border_width_all(3)
		sb.set_corner_radius_all(12)
		sb.shadow_color = Color(color, 0.25)
		sb.shadow_size = 8
		draw_style_box(sb, r)

## Rebuilds the card from the advisor's current guidance.
func update() -> void:
	for c in get_children():
		remove_child(c)
		c.queue_free()
	var show := not app.view.is_empty() and app.screen in ["galaxy", "system", "research", "combat", "fleets", "diplomacy", "empire", "intel", "database"]
	visible = show
	if not show:
		current = {}
		_highlight_target()
		return
	current = advisor.guidance()
	if current.is_empty() or not advisor.shown_on(current, app.screen):
		current = {}
		visible = false
		_highlight_target()
		return
	var col := Lcars.tone_color(str(current.get("tone", "primary")))
	var sb := Lcars.box_lcars(Color(Lcars.PANEL_SOLID, 0.96), 14, col, 1, Vector4(16, 12, 16, 12))
	sb.border_width_left = 8
	add_theme_stylebox_override("panel", sb)
	var v := Lcars.vbox(8)
	add_child(v)
	if current.get("beat", false):
		_beat_card(v, col)
		_place.call_deferred()
		return
	var head := Lcars.hbox(8)
	head.add_child(UiIcon.make("padd" if current.get("tutorial", false) else "info", 20, col))
	var t := Lcars.label(str(current["title"]).to_upper(), "", 18, col)
	t.name = "AdvisorTitle"
	t.add_theme_font_override("font", Lcars.font("display", 600))
	head.add_child(t)
	head.add_child(Lcars.spacer())
	if current.get("tutorial", false):
		head.add_child(Lcars.caption("TUTORIAL %d/%d" % [int(current["step"]), int(current["steps"])]))
	else:
		head.add_child(Lcars.caption("ADVISOR"))
	var fold := Lcars.pill("", "ghost", func(): folded = not folded; update(), "", "Fold the advisor")
	fold.text = "+" if folded else "-"
	fold.custom_minimum_size = Vector2(30, 26)
	fold.name = "AdvisorFold"
	head.add_child(fold)
	v.add_child(head)
	if folded:
		_place.call_deferred()
		return
	var body := Lcars.para(str(current["text"]), Lcars.TEXT, 15)
	body.name = "AdvisorText"
	body.custom_minimum_size.x = 368
	v.add_child(body)
	var row := Lcars.hbox(8)
	row.alignment = BoxContainer.ALIGNMENT_END
	if current.get("tutorial", false):
		var sk := Lcars.pill("Skip tutorial", "ghost", func(): advisor.skip_tutorial(); update())
		sk.name = "AdvisorDismiss"
		row.add_child(sk)
	elif current.get("dismiss", false):
		var nn := Lcars.pill("Not now", "ghost", func(): advisor.dismiss(current); update())
		nn.name = "AdvisorDismiss"
		row.add_child(nn)
	var act := Lcars.pill(str(current["act_label"]), "primary" if str(current.get("tone", "")) != "alert" else "alert", func(): press())
	act.name = "AdvisorAct"
	row.add_child(act)
	v.add_child(row)
	_place.call_deferred()

## A tutorial beat: progress pips, the one short line, and Skip. No button to press: the player does the thing.
func _beat_card(v: VBoxContainer, col: Color) -> void:
	if int(current.get("steps", 0)) > 0:
		var pips := Lcars.hbox(4)
		pips.name = "BeatPips"
		for k in int(current["steps"]):
			var pip := ColorRect.new()
			pip.custom_minimum_size = Vector2(22, 5)
			pip.color = col if k < int(current["step"]) else Color(col, 0.25)
			pips.add_child(pip)
		v.add_child(pips)
	var body := Lcars.para(str(current["text"]), Lcars.TEXT, 18)
	body.name = "AdvisorText"
	body.custom_minimum_size.x = 340
	v.add_child(body)
	var row := Lcars.hbox(8)
	row.alignment = BoxContainer.ALIGNMENT_END
	var sk := Lcars.pill("Skip tutorial", "ghost", func(): advisor.skip_tutorial(); update())
	sk.name = "AdvisorDismiss"
	row.add_child(sk)
	v.add_child(row)

## The action button: performs the guidance and moves on.
func press() -> String:
	if current.is_empty() or _busy:
		return "busy"
	_busy = true
	var g := current
	var why := advisor.perform(g)
	_busy = false
	if why != "":
		app.toast(why, "warn")
	_sig = ""
	if is_inside_tree():
		update()
	return why
