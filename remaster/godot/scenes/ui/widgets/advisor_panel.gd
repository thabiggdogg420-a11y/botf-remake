class_name AdvisorPanel
extends PanelContainer
## The advisor's LCARS card (F3): the tutorial step or the current hint, one action button, and "Not now" (or
## "Skip tutorial"). Docked bottom-left of the stage, above the map layer bar; hidden outside a campaign.
## Logic: Advisor (scenes/ui/advisor.gd).

var app: App
var advisor: Advisor
var current: Dictionary = {}
var folded := false
var _sig := ""
var _poll := 0.0
var _busy := false

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

## Bottom-left, above the map layer bar and clear of the toasts (which sit centre-bottom).
func _place() -> void:
	var vp := get_parent_area_size() if get_parent() else Vector2(1920, 1080)
	reset_size()
	position = Vector2(180, vp.y - size.y - 100)

## Rebuilds the card from the advisor's current guidance.
func update() -> void:
	for c in get_children():
		remove_child(c)
		c.queue_free()
	var show := not app.view.is_empty() and app.screen in ["galaxy", "system", "research", "combat", "fleets", "diplomacy", "empire", "intel", "database"]
	visible = show
	if not show:
		current = {}
		return
	current = advisor.guidance()
	if current.is_empty():
		visible = false
		return
	var col := Lcars.tone_color(str(current.get("tone", "primary")))
	var sb := Lcars.box_lcars(Color(Lcars.PANEL_SOLID, 0.96), 14, col, 1, Vector4(16, 12, 16, 12))
	sb.border_width_left = 8
	add_theme_stylebox_override("panel", sb)
	var v := Lcars.vbox(8)
	add_child(v)
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
